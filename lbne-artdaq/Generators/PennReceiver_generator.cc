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

lbne::PennReceiver::PennReceiver(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  run_receiver_(false)
{

  int fragment_id = ps.get<int>("fragment_id");
  fragment_ids_.push_back(fragment_id);

  instance_name_for_metrics_ = "PennReceiver";

  ////////////////////////////
  // HARDWARE OPTIONS

  // config stream connection parameters
  penn_client_host_addr_ =
	ps.get<std::string>("penn_client_host_addr", "localhost");
  penn_client_host_port_ =
	ps.get<std::string>("penn_client_host_port", "9999");
  penn_client_timeout_usecs_ =
	ps.get<uint32_t>("penn_client_timeout_usecs", 0);

  // Penn trigger options
  penn_mode_calibration_ =
    ps.get<bool>("penn_mode_calibration", false);
  penn_mode_external_triggers_ =
    ps.get<bool>("penn_mode_external_triggers", false);
  penn_mode_muon_triggers_ =
    ps.get<bool>("penn_mode_muon_triggers", false);

  // Penn hit masks
  penn_hit_mask_bsu_ =
    ps.get<uint64_t>("penn_hit_mask_bsu", 0x0003FFFFFFFFFFFF);
  penn_hit_mask_tsu_ =
    ps.get<uint64_t>("penn_hit_mask_tsu", 0x000000FFFFFFFFFF);
  
  // Penn microslice duration
  penn_data_microslice_size_ =
        ps.get<uint32_t>("penn_data_microslice_size", 7);

  // data stream connection parameters
  penn_data_dest_host_ =
	ps.get<std::string>("penn_data_dest_host", "127.0.0.1");
  penn_data_dest_port_ =
	ps.get<uint16_t>("penn_data_dest_port", 8989);

  ////////////////////////////
  // BOARDREADER OPTIONS

  // 
  receiver_tick_period_usecs_ =
    ps.get<uint32_t>("receiver_tick_period_usecs", 10000);

  // millislice size parameters
  millislice_size_ =
	ps.get<uint32_t>("millislice_size", 10);
  millislice_overlap_size_ = 
        ps.get<uint16_t>("millislice_overlap_size", 5);

  // boardreader printouts
  int receiver_debug_level =
	ps.get<int>("receiver_debug_level", 0);
  reporting_interval_fragments_ =
    ps.get<uint32_t>("reporting_interval_fragments", 100);
  reporting_interval_time_ = 
    ps.get<uint32_t>("reporting_interval_time", 0);

  // boardreader buffer sizes
  raw_buffer_size_ =
	ps.get<size_t>("raw_buffer_size", 10000);
  raw_buffer_precommit_ =
	ps.get<uint32_t>("raw_buffer_precommit", 10);
  use_fragments_as_raw_buffer_ =
    ps.get<bool>("use_fragments_as_raw_buffer", true);
#ifdef REBLOCK_PENN_USLICE
  if(use_fragments_as_raw_buffer_ == false) {
    mf::LogError("PennReceiver") << "use_fragments_as_raw_buffer == false has not been implemented";
    //TODO handle error cleanly here    
  }
#endif


  ////////////////////////////
  // EMULATOR OPTIONS

  // amount of data to generate
  penn_data_num_millislices_ =
	ps.get<uint32_t>("penn_data_num_millislices", 10);
  penn_data_num_microslices_ =
	ps.get<uint32_t>("penn_data_num_microslices", 10);
  penn_data_frag_rate_ =
	ps.get<float>("penn_data_frag_rate", 10.0);

  // type of data to generate
  penn_data_payload_mode_ =
	ps.get<uint16_t>("penn_data_payload_mode", 0);
  penn_data_trigger_mode_ =
	ps.get<uint16_t>("penn_data_trigger_mode", 0);
  penn_data_fragment_microslice_at_ticks_ =
        ps.get<int32_t>("penn_data_fragment_microslice_at_ticks", 0);

  // special debug options
  penn_data_repeat_microslices_ =
        ps.get<bool>("penn_data_repeat_microslices", false);
  penn_data_debug_partial_recv_ =
        ps.get<bool>("penn_data_debug_partial_recv", false);



  // Create an PENN client instance
  penn_client_ = std::unique_ptr<lbne::PennClient>(new lbne::PennClient(
		  penn_client_host_addr_, penn_client_host_port_, penn_client_timeout_usecs_));

#ifndef PENN_EMULATOR
  penn_client_->send_command("HardReset");
  sleep(1);
  std::ostringstream config_frag;
  this->generate_config_frag(config_frag);
  penn_client_->send_config(config_frag.str());
  bool rate_test = false;
#else
  bool rate_test = penn_data_repeat_microslices_;
#endif //!PENN_EMULATOR

  // Create a PennDataReceiver instance
  data_receiver_ = 
    std::unique_ptr<lbne::PennDataReceiver>(new lbne::PennDataReceiver(receiver_debug_level, receiver_tick_period_usecs_, penn_data_dest_port_,
								       millislice_size_, millislice_overlap_size_, rate_test));

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
	report_time_ = start_time_;

	// Start the data receiver
	data_receiver_->start();

#ifdef PENN_EMULATOR
	// Set up parameters in PENN emulator
	penn_client_->set_param("host",  penn_data_dest_host_, "str");
	penn_client_->set_param("port",  penn_data_dest_port_, "int");
	penn_client_->set_param("millislices", penn_data_num_millislices_, "int");
	penn_client_->set_param("microslices", penn_data_num_microslices_, "int");
	penn_client_->set_param("repeat_microslices", penn_data_repeat_microslices_, "int"); //emulator can't parse bool correctly
	penn_client_->set_param("rate",  penn_data_frag_rate_, "float");
	penn_client_->set_param("payload_mode", penn_data_payload_mode_, "int");
	penn_client_->set_param("trigger_mode", penn_data_trigger_mode_, "int");
	penn_client_->set_param("nticks_per_microslice", penn_data_microslice_size_, "int");
	penn_client_->set_param("fragment_microslice_at_ticks", penn_data_fragment_microslice_at_ticks_, "int");
	penn_client_->set_param("debug_partial_recv", penn_data_debug_partial_recv_, "int"); //emulator can't parse bool correctly

	// Send start command to PENN
	penn_client_->send_command("START");
#else

	penn_client_->send_command("SoftReset");
	penn_client_->send_command("SetRunState", "Enable");
#endif //PENN_EMULATOR

}

void lbne::PennReceiver::stop(void)
{
	mf::LogInfo("PennReceiver") << "stop() called";

	// Instruct the PENN to stop
#ifdef PENN_EMULATOR
	penn_client_->send_command("STOP");
#else
	penn_client_->send_command("SetRunState", "Stopped");
#endif //PENN_EMULATOR

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
    if (reporting_interval_time_ != 0) 
      {
	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - report_time_).count() >
	    (reporting_interval_time_ * 1000))
	  {
	    report_time_ = std::chrono::high_resolution_clock::now();
	    mf::LogInfo("PennReceiver") << "Received " << millislices_received_ << " millislices so far";
	  }
      }
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
		  millislice_size = validate_millislice_from_fragment_buffer(frag->dataBeginBytes(), recvd_buffer->size(),
#ifndef REBLOCK_PENN_USLICE
									     recvd_buffer->count(),
#endif
									     recvd_buffer->sequenceID(),
									     recvd_buffer->countPayload(), recvd_buffer->countPayloadCounter(),
									     recvd_buffer->countPayloadTrigger(), recvd_buffer->countPayloadTimestamp(),
									     recvd_buffer->endTimestamp(), recvd_buffer->widthTicks(), recvd_buffer->overlapTicks());

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

#ifndef REBLOCK_PENN_USLICE
	  // Format the raw digitisations (nanoslices) in the received buffer into a millislice
	  // within the data payload of the fragment
	  millislice_size = format_millislice_from_raw_buffer((uint16_t*)data_ptr, recvd_buffer->size(),
			  (uint8_t*)(frag->dataBeginBytes()), fragDataSize);
#endif

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

#ifndef REBLOCK_PENN_USLICE
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
#endif

uint32_t lbne::PennReceiver::validate_millislice_from_fragment_buffer(uint8_t* data_addr, size_t data_size,
#ifndef REBLOCK_PENN_USLICE
								      uint32_t us_count,
#endif
								      uint16_t millislice_id,
								      uint16_t payload_count, uint16_t payload_count_counter,
								      uint16_t payload_count_trigger, uint16_t payload_count_timestamp,
								      uint64_t end_timestamp, uint32_t width_in_ticks, uint32_t overlap_in_ticks)
{
	lbne::PennMilliSliceWriter millislice_writer(data_addr, data_size+sizeof(PennMilliSlice::Header));

	millislice_writer.finalize(true, data_size, 
#ifndef REBLOCK_PENN_USLICE
				   us_count,
#endif
				   millislice_id,
				   payload_count, payload_count_counter, payload_count_trigger, payload_count_timestamp,
				   end_timestamp, width_in_ticks, overlap_in_ticks);
	//TODO add a check here to make sure the size agrees with the payload counts + header size

	return millislice_writer.size();
}

void lbne::PennReceiver::generate_config_frag(std::ostringstream& config_frag) {

  config_frag << "<RunMode>" << std::endl
	      << " <Calibrations>" << (penn_mode_calibration_       ? "True" : "False") << "</Calibrations>" << std::endl
	      << " <ExtTriggers>"  << (penn_mode_external_triggers_ ? "True" : "False") << "</ExtTriggers>"  << std::endl
	      << " <MuonTriggers>" << (penn_mode_muon_triggers_     ? "True" : "False") << "</MuonTriggers>" << std::endl
	      << "</RunMode>" << std::endl;

  config_frag << "<MuonTriggers>" << std::endl
	      << " <HitMaskBSU>" << penn_hit_mask_bsu_ << "</HitMaskBSU>" << std::endl
	      << " <HitMaskTSU>" << penn_hit_mask_tsu_ << "</HitMaskTSU>" << std::endl
	      << "</MuonTriggers>" << std::endl;

  config_frag << "<DataBuffer>" << std::endl
	      << " <DaqHost>" << penn_data_dest_host_ << "</DaqHost>" << std::endl
	      << " <DaqPort>" << penn_data_dest_port_ << "</DaqPort>" << std::endl
	      << "</DataBuffer>" << std::endl;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::PennReceiver) 
