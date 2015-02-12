#include "lbne-artdaq/Generators/PennReceiver.hh"

#include "lbne-raw-data/Overlays/PennMilliSliceWriter.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <sstream>
#include <thread>

#include <unistd.h>

//#define NO_PENN_CLIENT 1
#define PENN_EMULATOR 1

lbne::PennReceiver::PennReceiver(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  run_receiver_(false)
{

  int fragment_id = ps.get<int>("fragment_id");
  fragment_ids_.push_back(fragment_id);

  // the following parameters are part of faking the hardware readout
  number_of_microslices_to_generate_ =
	ps.get<uint32_t>("number_of_microslices_to_generate", 1);
  number_of_values_to_generate_ =
	ps.get<uint32_t>("number_of_values_to_generate", 3);
  simulated_readout_time_usec_ =
	ps.get<uint32_t>("simulated_readout_time", 100000);

  dpm_client_host_addr_ =
	ps.get<std::string>("penn_client_host_addr", "localhost");
  dpm_client_host_port_ =
	ps.get<std::string>("penn_client_host_port", "9999");
  dpm_client_timeout_usecs_ =
	ps.get<uint32_t>("penn_client_timeout_usecs", 0);

  penn_xml_config_file_ = 
	ps.get<std::string>("penn_xml_config_file", "config.xml");
  penn_daq_mode_ =
	ps.get<std::string>("penn_daq_mode", "Trigger");

  penn_data_dest_host_ =
	ps.get<std::string>("penn_data_dest_host", "127.0.0.1");
  penn_data_dest_port_ =
	ps.get<uint16_t>("penn_data_dest_port", 8989);
  penn_data_num_millislices_ =
	ps.get<uint32_t>("penn_data_num_millislices", 10);
  penn_data_num_microslices_ =
	ps.get<uint32_t>("penn_data_num_microslices", 10);
  penn_data_repeat_microslices_ =
        ps.get<bool>("penn_data_repeat_microslices", false);
  penn_data_frag_rate_ =
	ps.get<float>("penn_data_frag_rate", 10.0);
  penn_data_payload_mode_ =
	ps.get<uint16_t>("penn_data_payload_mode", 0);
  penn_data_trigger_mode_ =
	ps.get<uint16_t>("penn_data_trigger_mode", 0);
  penn_data_nticks_per_microslice_ =
        ps.get<int32_t>("penn_data_nticks_per_microslice", 10);
  penn_data_fragment_microslice_at_ticks_ =
        ps.get<int32_t>("penn_data_fragment_microslice_at_ticks", -1);

  receive_port_ =
	ps.get<uint16_t>("receive_port", 9999);
  raw_buffer_size_ =
	ps.get<size_t>("raw_buffer_size", 10000);
  raw_buffer_precommit_ =
	ps.get<uint32_t>("raw_buffer_precommit", 10);

  use_fragments_as_raw_buffer_ =
	ps.get<bool>("use_fragments_as_raw_buffer", false);

  receiver_tick_period_usecs_ =
    ps.get<uint32_t>("receiver_tick_period_usecs", 10000);

  int receiver_debug_level =
	ps.get<int>("receiver_debug_level", 0);

  number_of_microslices_per_millislice_ =
	ps.get<uint16_t>("number_of_microslices_per_millislice", 10);

  reporting_interval_fragments_ =
    ps.get<uint32_t>("reporting_interval_fragments", 100);

  // Create an PENN client instance
#ifndef NO_PENN_CLIENT
  dpm_client_ = std::unique_ptr<lbne::PennClient>(new lbne::PennClient(
		  dpm_client_host_addr_, dpm_client_host_port_, dpm_client_timeout_usecs_));

#ifndef PENN_EMULATOR
  dpm_client_->send_command("HardReset");
  sleep(1);
  dpm_client_->send_command("ReadXmlFile", penn_xml_config_file_);
  std::ostringstream config_frag;
  config_frag << "<DataDpm><DaqMode>" << penn_daq_mode_ << "</DaqMode></DataDpm>";
  dpm_client_->send_config(config_frag.str());
#endif //!PENN_EMULATOR

#endif //!NO_PENN_CLIENT

  // Create a PennDataReceiver instance
  data_receiver_ = std::unique_ptr<lbne::PennDataReceiver>(new lbne::PennDataReceiver(
		  receiver_debug_level, receiver_tick_period_usecs_, receive_port_, number_of_microslices_per_millislice_));

}

void lbne::PennReceiver::start(void)
{
	mf::LogDebug("PennReceiver") << "start() called";

	// Tell the data receiver to drain any unused buffers from the empty queue
	data_receiver_->release_empty_buffers();

	// Tell the data receiver to release any unused filled buffers
	data_receiver_->release_filled_buffers();

	// Clear the fragment map of any pre-allocated fragments still present
	raw_to_frag_map_.clear();

	// Pre-commit buffers to the data receiver object - creating either fragments or raw buffers depending on raw buffer mode
	mf::LogDebug("PennReceiver") << "Pre-committing " << raw_buffer_precommit_ << " buffers of size " << raw_buffer_size_ << " to receiver";
	empty_buffer_low_mark_ = 0;
	for (unsigned int i = 0; i < raw_buffer_precommit_; i++)
	{
		lbne::PennRawBufferPtr raw_buffer;
		if (use_fragments_as_raw_buffer_)
		{
			raw_buffer = this->create_new_buffer_from_fragment();
		}
		else
		{
			raw_buffer = lbne::PennRawBufferPtr(new PennRawBuffer(raw_buffer_size_));
		}
		mf::LogDebug("PennReceiver") << "Pre-commiting raw buffer " << i << " at address " << (void*)(raw_buffer->dataPtr());
		data_receiver_->commit_empty_buffer(raw_buffer);
		empty_buffer_low_mark_++;
	}

	// Initialise data statistics
	millislices_received_ = 0;
	total_bytes_received_ = 0;
	start_time_ = std::chrono::high_resolution_clock::now();

	// Start the data receiver
	data_receiver_->start();

#ifndef NO_PENN_CLIENT

#ifdef PENN_EMULATOR
	// Set up parameters in PENN emulator
	dpm_client_->set_param("host",  penn_data_dest_host_, "str");
	dpm_client_->set_param("port",  penn_data_dest_port_, "int");
	dpm_client_->set_param("millislices", penn_data_num_millislices_, "int");
	dpm_client_->set_param("microslices", penn_data_num_microslices_, "int");
	dpm_client_->set_param("repeat_microslices", penn_data_repeat_microslices_, "int"); //should be bool
	dpm_client_->set_param("rate",  penn_data_frag_rate_, "float");
	dpm_client_->set_param("payload_mode", penn_data_payload_mode_, "int");
	dpm_client_->set_param("trigger_mode", penn_data_trigger_mode_, "int");
	dpm_client_->set_param("nticks_per_microslice", penn_data_nticks_per_microslice_, "int");
	dpm_client_->set_param("fragment_microslice_at_ticks", penn_data_fragment_microslice_at_ticks_, "int");

	// Send start command to PENN
	dpm_client_->send_command("START");
#else

	dpm_client_->send_command("SoftReset");
	dpm_client_->send_command("SetRunState", "Enable");
#endif //PENN_EMULATOR

#endif //!NO_PENN_CLIENT

}

void lbne::PennReceiver::stop(void)
{
	mf::LogInfo("PennReceiver") << "stop() called";

	// Instruct the PENN to stop
#ifndef NO_PENN_CLIENT

#ifdef PENN_EMULATOR
	dpm_client_->send_command("STOP");
#else
	dpm_client_->send_command("SetRunState", "Stopped");
#endif //PENN_EMULATOR

#endif //!NO_PENN_CLIENT

	// Stop the data receiver.
	data_receiver_->stop();

	mf::LogInfo("PennReceiver") << "Low water mark on empty buffer queue is " << empty_buffer_low_mark_;

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_received_) / elapsed_secs;
	double data_rate_mbs = ((double)total_bytes_received_) / ((1024*1024) * elapsed_secs);

	mf::LogInfo("PennReceiver") << "Received " << millislices_received_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz, total data " << total_bytes_received_ << " bytes, rate " << data_rate_mbs << " MB/s";

}


bool lbne::PennReceiver::getNext_(artdaq::FragmentPtrs & frags) {

  PennRawBufferPtr recvd_buffer;
  bool buffer_available = false;

  // Wait for a filled buffer to be available, with a timeout tick to allow the should_stop() state
  // to be checked
  do
  {
    buffer_available = data_receiver_->retrieve_filled_buffer(recvd_buffer, 500000);
  }
  while (!buffer_available && !should_stop());

  // If stopping, Check if there are any filled buffers available (i.e. fragments received but not processed)
  // and print a warning, then return false to indicate no more fragments to process
  if (should_stop()) {

		if( data_receiver_->filled_buffers_available() > 0)
		{
			mf::LogWarning("PennRecevier") << "getNext_ stopping while there were still filled buffers available";
		}
		else
		{
		        mf::LogInfo("PennReceiver") << "No unprocessed filled buffers available at end of run";
		}

	return false;
  }


  // If there was no data received (or an error flagged), simply return
  // an empty list
  if (recvd_buffer->size() == 0)
  {
	  mf::LogWarning("PennReceiver") << "getNext_ : no data received in raw buffer";
	  return true;
  }

  uint8_t* data_ptr = recvd_buffer->dataPtr();

  std::unique_ptr<artdaq::Fragment> frag;
  uint32_t millislice_size = 0;

  if (use_fragments_as_raw_buffer_)
  {
	  // Map back onto the fragment from the raw buffer data pointer we have just received
	  if (raw_to_frag_map_.count(data_ptr))
	  {
		  // Extract the fragment from the map
		  frag = std::move(raw_to_frag_map_[data_ptr]);

		  // Validate and finalize the fragment received
		  millislice_size = validate_millislice_from_fragment_buffer(frag->dataBeginBytes(), recvd_buffer->size(), recvd_buffer->count());

		  // Clean up entry in map to remove raw fragment buffer
		  raw_to_frag_map_.erase(data_ptr);

		  // Create a new raw buffer pointing at a new fragment and replace the received buffer
		  // pointer with it - this will be recycled onto the empty queue later
		  recvd_buffer = create_new_buffer_from_fragment();

	  }
	  else
	  {
		  mf::LogError("PennReciver") << "Cannot map raw buffer with data address" << (void*)recvd_buffer->dataPtr() << " back onto fragment";
		  return true;
	  }
  }
  else
  {
	  // Create an artdaq::Fragment to format the raw data into. As a crude heuristic,
	  // reserve 10 times the raw data space in the fragment for formatting overhead.
	  // TODO This needs to be done more intelligently!
	  size_t fragDataSize = recvd_buffer->size() * 10;
	  frag = artdaq::Fragment::FragmentBytes(fragDataSize);

	  // Format the raw digitisations (nanoslices) in the received buffer into a millislice
	  // within the data payload of the fragment
	  millislice_size = format_millislice_from_raw_buffer((uint16_t*)data_ptr, recvd_buffer->size(),
			  (uint8_t*)(frag->dataBeginBytes()), fragDataSize);

  }

  // Update statistics counters
  millislices_received_++;
  total_bytes_received_ += millislice_size;

  if ((millislices_received_ % reporting_interval_fragments_) == 0)
  {
	  auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	  double elapsed_secs = ((double)elapsed_msecs)/1000;

	  mf::LogInfo("PennReceiver") << "Received " << millislices_received_ << " millislices, "
			  << float(total_bytes_received_)/(1024*1024) << " MB in " << elapsed_secs << " seconds";
  }

  // Recycle the raw buffer onto the commit queue for re-use by the receiver.
  // TODO at this point we could test the number of unused buffers on the commit queue
  // against a low water mark and create/inject ones to keep the receiver running if necessary
  size_t empty_buffers_available = data_receiver_->empty_buffers_available();
  if (empty_buffers_available < empty_buffer_low_mark_)
  {
	  empty_buffer_low_mark_ = empty_buffers_available;
  }

  data_receiver_->commit_empty_buffer(recvd_buffer);

  // Set fragment fields appropriately
  frag->setSequenceID(ev_counter());
  frag->setFragmentID(fragmentIDs()[0]);
  frag->setUserType(lbne::detail::TRIGGER);

  // Resize fragment to final millislice size
  frag->resizeBytes(millislice_size);

  // add the fragment to the list
  frags.emplace_back(std::move (frag));

  // increment the event counter
  ev_counter_inc();

  return true;
}


lbne::PennRawBufferPtr lbne::PennReceiver::create_new_buffer_from_fragment(void)
{
	PennRawBufferPtr raw_buffer;

	std::unique_ptr<artdaq::Fragment> frag = artdaq::Fragment::FragmentBytes(raw_buffer_size_ + sizeof(PennMilliSlice::Header));
	uint8_t* data_ptr = frag->dataBeginBytes() + sizeof(PennMilliSlice::Header);
	raw_buffer = lbne::PennRawBufferPtr(new PennRawBuffer(data_ptr, raw_buffer_size_));

	raw_to_frag_map_.insert(std::pair<uint8_t*, std::unique_ptr<artdaq::Fragment> >(raw_buffer->dataPtr(), std::move(frag)));

	return raw_buffer;

}

uint32_t lbne::PennReceiver::format_millislice_from_raw_buffer(uint16_t* src_addr, size_t src_size,
		                                                         uint8_t* dest_addr, size_t dest_size)
{
  const uint32_t MICROSLICE_BUFFER_SIZE = 512;

  std::shared_ptr<lbne::PennMicroSliceWriter> microslice_writer_ptr;

  uint16_t* sample_addr = src_addr;

  lbne::PennMilliSliceWriter millislice_writer(dest_addr, dest_size);
  for (uint32_t udx = 0; udx < number_of_microslices_to_generate_; ++udx) {
    microslice_writer_ptr = millislice_writer.reserveMicroSlice(MICROSLICE_BUFFER_SIZE);
    if (microslice_writer_ptr.get() == 0) {
      mf::LogError("PennReceiver")
        << "Unable to create microslice number " << udx;
    }
    else {
      //get the size of the microslice from the header 
      // and divide by 2 (unit in header is bytes, but working with uint16_t)
      uint16_t microslice_size = sample_addr[1] / 2;
      //add the microslice to the microslice writer
      microslice_writer_ptr->addData(sample_addr, microslice_size);
      microslice_writer_ptr->finalize();
      //move to the next microslice
      sample_addr += microslice_size;
    }
  }

  // Check if we have overrun the end of the raw buffer
  if (sample_addr > (src_addr + src_size))
  {
	  mf::LogError("PennReceiver")
	  	  << "Raw buffer overrun during millislice formatting by " << ((src_addr + src_size) - sample_addr);
  }
  millislice_writer.finalize();
  return millislice_writer.size();

}

uint32_t lbne::PennReceiver::validate_millislice_from_fragment_buffer(uint8_t* data_addr, size_t data_size, uint32_t count)
{
	lbne::PennMilliSliceWriter millislice_writer(data_addr, data_size+sizeof(PennMilliSlice::Header));

	millislice_writer.finalize(true, data_size, count);
	return millislice_writer.size();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::PennReceiver) 
