#include "lbne-artdaq/Generators/TpcRceReceiver.hh"
#include "lbne-raw-data/Overlays/MilliSliceWriter.hh"
#include "lbne-raw-data/Overlays/TpcMilliSliceWriter.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "lbne-raw-data/Overlays/MilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/MilliSliceFragmentWriter.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"
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


lbne::TpcRceReceiver::TpcRceReceiver(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  run_receiver_(false)
{

  int fragment_id = ps.get<int>("fragment_id");
  fragment_ids_.push_back(fragment_id);

  // the following parameters are part of faking the hardware readout
  number_of_microslices_to_generate_ =
	ps.get<uint32_t>("number_of_microslices_to_generate", 1);
  number_of_nanoslices_to_generate_ =
	ps.get<uint32_t>("number_of_nanoslices_to_generate", 1);
  number_of_values_to_generate_ =
	ps.get<uint32_t>("number_of_values_to_generate", 3);
  simulated_readout_time_usec_ =
	ps.get<uint32_t>("simulated_readout_time", 100000);

  rce_client_host_addr_ =
	ps.get<std::string>("rce_client_host_addr", "localhost");
  rce_client_host_port_ =
	ps.get<std::string>("rce_client_host_port", "9999");
  rce_client_timeout_usecs_ =
	ps.get<uint32_t>("rce_client_timeout_usecs", 0);

  rce_data_dest_host_ =
	ps.get<std::string>("rce_data_dest_host", "127.0.0.1");
  rce_data_dest_port_ =
	ps.get<uint16_t>("rce_data_dest_port", 8989);
  rce_data_num_millislices_ =
	ps.get<uint32_t>("rce_data_num_millislices", 10);
  rce_data_num_microslices_ =
	ps.get<uint32_t>("rce_data_num_microslices", 10);
  rce_data_frag_rate_ =
	ps.get<float>("rce_data_frag_rate", 10.0);
  rce_data_adc_mode_ =
	ps.get<uint16_t>("rce_data_adc_mode", 0);
  rce_data_adc_mean_ =
	ps.get<float>("rce_data_adc_mean", 1000.0);
  rce_data_adc_sigma_ =
	ps.get<float>("rce_data_adc_sigma", 100.0);

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

  // Create an RCE client instance
  rce_client_ = std::unique_ptr<lbne::RceClient>(new lbne::RceClient(
		  rce_client_host_addr_, rce_client_host_port_, rce_client_timeout_usecs_));

  // Create a RceDataReceiver instance
  data_receiver_ = std::unique_ptr<lbne::RceDataReceiver>(new lbne::RceDataReceiver(
		  receiver_debug_level, receiver_tick_period_usecs_, receive_port_, number_of_microslices_per_millislice_));

}

void lbne::TpcRceReceiver::start(void)
{
	mf::LogDebug("TpcRceReceiver") << "start() called";

	// Tell the data receiver to drain any unused buffers from the empty queue
	data_receiver_->release_empty_buffers();

	// Tell the data receiver to release any unused filled buffers
	data_receiver_->release_filled_buffers();

	// Clear the fragment map of any pre-allocated fragments still present
	raw_to_frag_map_.clear();

	// Pre-commit buffers to the data receiver object - creating either fragments or raw buffers depending on raw buffer mode
	mf::LogDebug("TpcRceReceiver") << "Pre-committing " << raw_buffer_precommit_ << " buffers of size " << raw_buffer_size_ << " to receiver";
	empty_buffer_low_mark_ = 0;
	for (unsigned int i = 0; i < raw_buffer_precommit_; i++)
	{
		lbne::RceRawBufferPtr raw_buffer;
		if (use_fragments_as_raw_buffer_)
		{
			raw_buffer = this->create_new_buffer_from_fragment();
		}
		else
		{
			raw_buffer = lbne::RceRawBufferPtr(new RceRawBuffer(raw_buffer_size_));
		}
		// mf::LogDebug("TpcRceReceiver") << "Pre-commiting raw buffer " << i << " at address " << (void*)(raw_buffer->dataPtr());
		data_receiver_->commit_empty_buffer(raw_buffer);
		empty_buffer_low_mark_++;
	}

	// Initialise data statistics
	millislices_received_ = 0;
	total_bytes_received_ = 0;
	start_time_ = std::chrono::high_resolution_clock::now();

	// Start the data receiver
	data_receiver_->start();

	// Set up parameters in RCE
	rce_client_->set_param("host",  rce_data_dest_host_, "str");
	rce_client_->set_param("port",  rce_data_dest_port_, "int");
	rce_client_->set_param("millislices", rce_data_num_millislices_, "int");
	rce_client_->set_param("microslices", rce_data_num_microslices_, "int");
	rce_client_->set_param("rate",  rce_data_frag_rate_, "float");
	rce_client_->set_param("adcmode", rce_data_adc_mode_, "int");
	rce_client_->set_param("adcmean", rce_data_adc_mean_, "float");
	rce_client_->set_param("adcsigma", rce_data_adc_sigma_, "float");

	// Send start command to RCE
	rce_client_->send_command("START");

}

void lbne::TpcRceReceiver::stop(void)
{
	mf::LogInfo("TpcRceReceiver") << "stop() called";

	// Instruct the RCE to stop
	rce_client_->send_command("STOP");

	// Stop the data receiver.
	data_receiver_->stop();

	mf::LogInfo("TpcRceReceiver") << "Low water mark on empty buffer queue is " << empty_buffer_low_mark_;

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_received_) / elapsed_secs;
	double data_rate_mbs = ((double)total_bytes_received_) / ((1024*1024) * elapsed_secs);

	mf::LogInfo("TpcRceReceiver") << "Received " << millislices_received_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz, total data " << total_bytes_received_ << " bytes, rate " << data_rate_mbs << " MB/s" << std::endl;

}


bool lbne::TpcRceReceiver::getNext_(artdaq::FragmentPtrs & frags) {

  RceRawBufferPtr recvd_buffer;
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
			mf::LogWarning("TpcRceRecevier") << "getNext_ stopping while there were still filled buffers available";
		}
		else
		{
	    	mf::LogInfo("TpcRceReceiver") << "No unprocessed filled buffers available at end of run";
		}

	return false;
  }


  // If there was no data received (or an error flagged), simply return
  // an empty list
  if (recvd_buffer->size() == 0)
  {
	  mf::LogWarning("TpcRceReceiver") << "getNext_ : no data received in raw buffer";
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
		  recvd_buffer =create_new_buffer_from_fragment();

	  }
	  else
	  {
		  mf::LogError("TpcRceReciver") << "Cannot map raw buffer with data address" << (void*)recvd_buffer->dataPtr() << " back onto fragment";
		  return true;
	  }
  }
  else
  {
	  // Create an artdaq::Fragment to format the raw data into. As a crude heuristic,
	  // reserve 10 times the raw data space in the fragment for formatting overhead.
	  // This needs to be done more intelligently!
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

	  mf::LogInfo("TpcRceReceiver") << "Received " << millislices_received_ << " millislices, "
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
  frag->setUserType(lbne::detail::TPC);

  // Resize fragment to final millislice size
  frag->resizeBytes(millislice_size);

  // add the fragment to the list
  frags.emplace_back(std::move (frag));

  // increment the event counter
  ev_counter_inc();

  return true;
}


lbne::RceRawBufferPtr lbne::TpcRceReceiver::create_new_buffer_from_fragment(void)
{
	RceRawBufferPtr raw_buffer;

	std::unique_ptr<artdaq::Fragment> frag = artdaq::Fragment::FragmentBytes(raw_buffer_size_ + sizeof(TpcMilliSlice::Header));
	uint8_t* data_ptr = frag->dataBeginBytes() + sizeof(TpcMilliSlice::Header);
	raw_buffer = lbne::RceRawBufferPtr(new RceRawBuffer(data_ptr, raw_buffer_size_));

	raw_to_frag_map_.insert(std::pair<uint8_t*, std::unique_ptr<artdaq::Fragment> >(raw_buffer->dataPtr(), std::move(frag)));

	return raw_buffer;

}

uint32_t lbne::TpcRceReceiver::format_millislice_from_raw_buffer(uint16_t* src_addr, size_t src_size,
		                                                         uint8_t* dest_addr, size_t dest_size)
{
  const uint16_t CHANNEL_NUMBER = 128;
  const uint32_t MICROSLICE_BUFFER_SIZE = 512;
  const uint32_t NANOSLICE_BUFFER_SIZE = 128;

  std::shared_ptr<lbne::MicroSliceWriter> microslice_writer_ptr;
  std::shared_ptr<lbne::NanoSliceWriter> nanoslice_writer_ptr;

  uint16_t channel_number = CHANNEL_NUMBER;

  uint16_t* sample_addr = src_addr;

  lbne::MilliSliceWriter millislice_writer(dest_addr, dest_size);
  for (uint32_t udx = 0; udx < number_of_microslices_to_generate_; ++udx) {
    microslice_writer_ptr = millislice_writer.reserveMicroSlice(MICROSLICE_BUFFER_SIZE);
    if (microslice_writer_ptr.get() == 0) {
      mf::LogError("TpcRceReceiver")
        << "Unable to create microslice number " << udx;
    }
    else {
      for (uint32_t ndx = 0; ndx < number_of_nanoslices_to_generate_; ++ndx) {
        nanoslice_writer_ptr = microslice_writer_ptr->reserveNanoSlice(NANOSLICE_BUFFER_SIZE);
        if (nanoslice_writer_ptr.get() == 0) {
          mf::LogError("TpcRceReceiver")
            << "Unable to create nanoslice number " << ndx;
        }
        else {
          nanoslice_writer_ptr->setChannelNumber(channel_number++);
          for (uint32_t sdx = 0; sdx < number_of_values_to_generate_; ++sdx) {
            nanoslice_writer_ptr->addSample(*sample_addr);
            sample_addr++;
          }
        }
      }
    }
  }

  // Check if we have overrun the end of the raw buffer
  if (sample_addr > (src_addr + src_size))
  {
	  mf::LogError("TpcRceReceiver")
	  	  << "Raw buffer overrun during millislice formatting by " << ((src_addr + src_size) - sample_addr);
  }
  millislice_writer.finalize();
  return millislice_writer.size();

}

uint32_t lbne::TpcRceReceiver::validate_millislice_from_fragment_buffer(uint8_t* data_addr, size_t data_size, uint32_t count)
{

	lbne::TpcMilliSliceWriter millislice_writer(data_addr, data_size+sizeof(TpcMilliSlice::Header));

	millislice_writer.finalize(data_size, count);
	return millislice_writer.size();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::TpcRceReceiver) 
