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

  ///
  /// Penn board options
  ///

  // -- data stream connection parameters
  penn_data_dest_host_ =
  ps.get<std::string>("penn_data_buffer.daq_host", "127.0.0.1");
  penn_data_dest_port_ =
  ps.get<uint16_t>("penn_data_buffer.daq_port", 8989);
  // Penn microslice duration
  penn_data_microslice_size_ =
        ps.get<uint32_t>("penn_data_buffer.daq_microslice_size", 7);


  // -- Channel masks
  penn_channel_mask_bsu_ =
    ps.get<uint64_t>("channel_mask.BSU", 0x0003FFFFFFFFFFFF);
  penn_channel_mask_tsu_ =
    ps.get<uint64_t>("channel_mask.BSU", 0x000000FFFFFFFFFF);

  // -- How to deal with external triggers
  penn_ext_triggers_mask_ = ps.get<uint8_t>("external_triggers.mask",0x1F);
  penn_ext_triggers_echo_ = ps.get<bool>("external_triggers.echo_triggers",true);
  penn_ext_triggers_echo_width_ = ps.get<uint8_t>("external_triggers.echo_width",2);

  // -- Calibrations
  penn_calib_period_ = ps.get<uint16_t>("calibration.period",10);
  penn_calib_channel_mask_ = ps.get<uint8_t>("calibration.channel_mask",0xF);
  penn_calib_pulse_width_ = ps.get<uint8_t>("calibration.pulse_width",2);

  // -- Muon triggers
  // This is the more elaborated part:
  // First grab the global parameters
  penn_muon_num_triggers_ = ps.get<uint32_t>("muon_triggers.num_triggers",4);
  penn_trig_out_pulse_width_ = ps.get<uint8_t>("muon_triggers.trig_out_width",2);
  penn_trig_in_window_ = ps.get<uint8_t>("muon_triggers.trig_window",3);
  penn_trig_lockdown_window_ = ps.get<uint8_t>("muon_triggers.trig_lockdown",3);

  // And now grab the individual trigger mask configuration
  for (uint32_t i = 0; i < penn_muon_num_triggers_; ++i) {
    TriggerMaskConfig mask;
    std::ostringstream trig_name("muon_triggers.trigger_");
    trig_name << i;
    mask.id           = ps.get<std::string>(trig_name.str() + ".id");
    mask.id_mask      = ps.get<std::string>(trig_name.str() + ".id_mask");
    mask.prescale     = ps.get<uint8_t>(trig_name.str() + ".prescale");
    mask.logic        = ps.get<uint8_t>(trig_name.str() + ".logic");
    mask.g1_logic     = ps.get<uint8_t>(trig_name.str() + ".group1.logic");
    mask.g1_mask_bsu  = ps.get<uint64_t>(trig_name.str() + ".group1.BSU");
    mask.g1_mask_tsu  = ps.get<uint64_t>(trig_name.str() + ".group1.TSU");
    mask.g2_logic     = ps.get<uint8_t>(trig_name.str() + ".group2.logic");
    mask.g2_mask_bsu  = ps.get<uint64_t>(trig_name.str() + ".group2.BSU");
    mask.g2_mask_tsu  = ps.get<uint64_t>(trig_name.str() + ".group2.TSU");

    muon_triggers_.push_back(mask);

  }


   ///
   /// -- Configuration loaded.
   ///


  // Create an PENN client instance
  penn_client_ = std::unique_ptr<lbne::PennClient>(new lbne::PennClient(
		  penn_client_host_addr_, penn_client_host_port_, penn_client_timeout_usecs_));

  // What does this actually do? FLushes the registers?
  penn_client_->send_command("HardReset");
  sleep(1);
  std::ostringstream config_frag;
  this->generate_config_frag(config_frag);
  penn_client_->send_config(config_frag.str());

#ifndef PENN_EMULATOR
  bool rate_test = false;
#else
  bool rate_test = penn_data_repeat_microslices_;

  //TODO check if it'd confuse the Penn to put emulator options in the XML file
  config_frag.str(std::string());
  this->generate_config_frag_emulator(config_frag);
  penn_client_->send_config(config_frag.str());
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

	// Send start command to PENN
	penn_client_->send_command("SoftReset");
	penn_client_->send_command("StartRun");

}

void lbne::PennReceiver::stop(void)
{
	mf::LogInfo("PennReceiver") << "stop() called";

	// Instruct the PENN to stop
	penn_client_->send_command("StopRun");

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
  // I don't understand what is going on here. Has to be
  // restructured for the configuration currently being used in the PTBreader software

  // The config wrapper is added by the PennClient class just prior to send.
 // config_frag << "<config>";

  // -- DataBuffer section. Controls the reader itself
  config_frag << "<DataBuffer>"
      << "<DaqHost>" << penn_data_dest_host_ << "</DaqHost>"
      << "<DaqPort>" << penn_data_dest_port_ << "</DaqPortt>"
      // FIXME: Add missing variables
      // Should we have a rollover or just use a microslice size for time?
      << "<RollOver>" << penn_data_dest_rollover_ << "</RollOver>"
      << "<MicroSliceDuration>" << penn_data_microslice_size_ << "</MicroSliceDuration>"
      << "</DataBuffer>";

  // -- Channel masks section. Controls the reader itself
  config_frag << "<ChannelMask>"
      << "<BSU>0x" << std::hex << penn_channel_mask_bsu_ << std::dec << "</BSU>"
      << "<TSU>0x" << std::hex << penn_channel_mask_tsu_ << std::dec << "</TSU>"
      << "</ChannelMask>";

  config_frag << "<MuonTriggers num_triggers=\"" << penn_muon_num_triggers_ << "\">"
      << "<TrigOutWidth>0x" << std::hex << penn_trig_out_pulse_width_  << std::dec << "</TrigOutWidth>"
      << "<TriggerWindow>0x" << std::hex << penn_trig_in_window_  << std::dec << "</TriggerWindow>"
      << "<LockdownWindow>0x" << std::hex << penn_trig_lockdown_window_  << std::dec << "</LockdownWindow>";
  for (size_t i = 0; i < penn_muon_num_triggers_; ++i) {
    // I would rather put masks as hex strings to be easier to understand
    config_frag << "<TriggerMask id=\"" << muon_triggers_.at(i).id << "\" mask=\"" <<muon_triggers_.at(i).id_mask <<  "\">"
        << "<ExtLogic>0x" << std::hex << muon_triggers_.at(i).logic << std::dec << "</ExtLogic>"
        << "<Prescale>" << muon_triggers_.at(i).prescale << "</Prescale>"
        << "<group1><Logic>0x" << std::hex << muon_triggers_.at(i).g1_logic << std::dec << "</Logic>"
        << "<BSU>0x" << std::hex << muon_triggers_.at(i).g1_mask_bsu << std::dec << "</BSU>"
        << "<TSU>0x" << std::hex << muon_triggers_.at(i).g1_mask_tsu << std::dec << "</TSU></group1>"
        << "<group2><Logic>0x" << std::hex << muon_triggers_.at(i).g2_logic << std::dec << "</Logic>"
        << "<BSU>0x" << std::hex << muon_triggers_.at(i).g2_mask_bsu << std::dec << "</BSU>"
        << "<TSU>0x" << std::hex << muon_triggers_.at(i).g2_mask_tsu << std::dec << "</TSU></group2>"
        << "</TriggerMask>";
  }

  // And finally close the tag
  //config_frag << "</config>";

}

void lbne::PennReceiver::generate_config_frag_emulator(std::ostringstream& config_frag) {

  config_frag << "<Emulator>" << " "
	      << " <NumMillislices>" << penn_data_num_millislices_ << "</NumMillislices>" << " "
	      << " <NumMicroslices>" << penn_data_num_microslices_ << "</NumMicroslices>" << " "
	      << " <SendRate>"       << penn_data_frag_rate_       << "</SendRate>"       << " "
	      << " <PayloadMode>"    << penn_data_payload_mode_    << "</PayloadMode>"    << " "
	      << " <TriggerMode>"    << penn_data_trigger_mode_    << "</TriggerMode>"    << " "
	      << " <FragmentUSlice>" << penn_data_fragment_microslice_at_ticks_ << "</FragmentUSlice>" << " "
	      << " <SendRepeats>"    << penn_data_repeat_microslices_           << "</SendRepeats>" << " "
	      << " <SendByByte>"     << penn_data_debug_partial_recv_           << "</SendByByte>" << " "
	      << "</Emulator>" << " ";
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::PennReceiver) 
