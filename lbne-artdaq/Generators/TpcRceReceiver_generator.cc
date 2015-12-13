#include "lbne-artdaq/Generators/TpcRceReceiver.hh"
#include "lbne-raw-data/Overlays/MilliSliceWriter.hh"
#include "lbne-raw-data/Overlays/TpcMilliSliceWriter.hh"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"

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

//#define NO_RCE_CLIENT 1

lbne::TpcRceReceiver::TpcRceReceiver(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  instance_name_("TpcRceReceiver"),
  run_receiver_(false),
  data_timeout_usecs_(ps.get<uint32_t>("data_timeout_usecs", 60000000))
{

  int board_id = ps.get<int>("board_id", 0);
  std::stringstream instance_name_ss;
  instance_name_ss << instance_name_ << board_id;
  instance_name_ = instance_name_ss.str();

  instance_name_for_metrics_ = "RCE " + boost::lexical_cast<std::string>(board_id);

  empty_buffer_low_water_metric_name_ = instance_name_for_metrics_ + " Empty Buffer Low Water Mark";
  empty_buffer_available_metric_name_ = instance_name_for_metrics_ + " Empty Buffers Available";
  filled_buffer_high_water_metric_name_ = instance_name_for_metrics_ + " Filled Buffer High Water Mark";
  filled_buffer_available_metric_name_ = instance_name_for_metrics_ + " Filled Buffers Available";

  DAQLogger::LogInfo(instance_name_) << "Starting up";

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

  dpm_client_host_addr_ =
	ps.get<std::string>("rce_client_host_addr", "localhost");
  dpm_client_host_port_ =
	ps.get<std::string>("rce_client_host_port", "9999");
  dpm_client_timeout_usecs_ =
	ps.get<uint32_t>("rce_client_timeout_usecs", 0);

  dtm_client_enable_ = 
        ps.get<bool>("dtm_client_enable", false);
  dtm_client_host_addr_ =
	ps.get<std::string>("dtm_client_host_addr", "localhost");
  dtm_client_host_port_ =
	ps.get<std::string>("dtm_client_host_port", "9999");
  dtm_client_timeout_usecs_ =
	ps.get<uint32_t>("dtm_client_timeout_usecs", 0);

  rce_xml_config_file_ = 
	ps.get<std::string>("rce_xml_config_file", "config.xml");
  rce_daq_mode_ =
	ps.get<std::string>("rce_daq_mode", "Trigger");
  rce_feb_emulation_ =
    ps.get<bool>("rce_feb_emulation_mode", false);

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
  filled_buffer_release_max_ =
	ps.get<uint32_t>("filled_buffer_release_max", 10);

  use_fragments_as_raw_buffer_ =
	ps.get<bool>("use_fragments_as_raw_buffer", false);

  receiver_tick_period_usecs_ =
    ps.get<uint32_t>("receiver_tick_period_usecs", 10000);

  int receiver_debug_level =
	ps.get<int>("receiver_debug_level", 0);

  number_of_microslices_per_millislice_ =
	ps.get<uint16_t>("number_of_microslices_per_millislice", 10);

  number_of_microslices_per_trigger_ =
	ps.get<uint16_t>("number_of_microslices_per_trigger", 10);

  reporting_interval_fragments_ =
    ps.get<uint32_t>("reporting_interval_fragments", 100);

  reporting_interval_time_ = 
    ps.get<uint32_t>("reporting_interval_time", 0);

#ifndef NO_RCE_CLIENT

  // If the DTM client is enabled (for standalone testing of the RCE), open
  // the connection, reset the DTM and enable timing emulation mode
  if (dtm_client_enable_) 
  {
    dtm_client_ = std::unique_ptr<lbne::RceClient>(new lbne::RceClient(instance_name_,
	       dtm_client_host_addr_, dtm_client_host_port_, dtm_client_timeout_usecs_));
  
    //dtm_client_->send_command("HardReset");
    //dtm_client_->send_command("ReadXmlFile", rce_xml_config_file_);
    //std::ostringstream config_frag;
    //config_frag << "<TimingDtm><TimingRtm><EmulationEnable>False</EmulationEnable></TimingRtm></TimingDtm>";
    //dtm_client_->send_config(config_frag.str());
  }

  // Create a client connection to the DPM
  dpm_client_ = std::unique_ptr<lbne::RceClient>(new lbne::RceClient(instance_name_,
		  dpm_client_host_addr_, dpm_client_host_port_, dpm_client_timeout_usecs_));

  // Set the DPM to FEB emulation mode if necessary
  if (rce_feb_emulation_) {
    DAQLogger::LogDebug(instance_name_) << "Enabling FEB emulation in RCE";
    dpm_client_->send_command("StartDebugFebEmu");
  }
  // else
  // {
  //   DAQLogger::LogDebug(instance_name_) << "Disabling FEB emulation in RCE";
  //   dpm_client_->send_command("StopDebugFebEmu");
  // }

  // Send a HardReset command to the DPM
  dpm_client_->send_command("HardReset");

  // Tell the DPM to read its configuration file
  dpm_client_->send_command("ReadXmlFile", rce_xml_config_file_);
  dpm_client_->send_command("SoftReset");

  dpm_client_->send_command("ConfigFebAsic");

  // Set the DPM run mode as specified
  std::ostringstream config_frag;
  config_frag << "<DataDpm><DataBuffer><RunMode>" << rce_daq_mode_ << "</RunMode></DataBuffer></DataDpm>";
  dpm_client_->send_config(config_frag.str());

#endif

  // Create a RceDataReceiver instance
  data_receiver_ = std::unique_ptr<lbne::RceDataReceiver>(new lbne::RceDataReceiver(instance_name_,
		  receiver_debug_level, receiver_tick_period_usecs_, receive_port_, number_of_microslices_per_millislice_));

}

void lbne::TpcRceReceiver::start(void)
{
	DAQLogger::LogDebug(instance_name_) << "start() called";

	// Tell the data receiver to drain any unused buffers from the empty queue
	data_receiver_->release_empty_buffers();

	// Tell the data receiver to release any unused filled buffers
	data_receiver_->release_filled_buffers();

	// Clear the fragment map of any pre-allocated fragments still present
	raw_to_frag_map_.clear();

	// Pre-commit buffers to the data receiver object - creating either fragments or raw buffers depending on raw buffer mode
	DAQLogger::LogDebug(instance_name_) << "Pre-committing " << raw_buffer_precommit_ << " buffers of size " << raw_buffer_size_ << " to receiver";
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
		// DAQLogger::LogDebug(instance_name_) << "Pre-commiting raw buffer " << i << " at address " << (void*)(raw_buffer->dataPtr());
		data_receiver_->commit_empty_buffer(raw_buffer);
		empty_buffer_low_mark_++;
	}
	filled_buffer_high_mark_ = 0;

	// Initialise data statistics
	millislices_received_ = 0;
	total_bytes_received_ = 0;
	start_time_ = std::chrono::high_resolution_clock::now();
	report_time_ = start_time_;

	// Start the data receiver
	data_receiver_->start();

#ifndef NO_RCE_CLIENT
	// Set up parameters in RCE
	// dpm_client_->set_param("host",  rce_data_dest_host_, "str");
	// dpm_client_->set_param("port",  rce_data_dest_port_, "int");
	// dpm_client_->set_param("millislices", rce_data_num_millislices_, "int");
	// dpm_client_->set_param("microslices", rce_data_num_microslices_, "int");
	// dpm_client_->set_param("rate",  rce_data_frag_rate_, "float");
	// dpm_client_->set_param("adcmode", rce_data_adc_mode_, "int");
	// dpm_client_->set_param("adcmean", rce_data_adc_mean_, "float");
	// dpm_client_->set_param("adcsigma", rce_data_adc_sigma_, "float");

	// // Send start command to RCE
	// dpm_client_->send_command("START");

	if (dtm_client_enable_) 
        {
	  //dtm_client_->send_command("SetRunState", "Enable");
	  //dtm_client_->send_command("Start");
	}

	// Set the run state to enabled
	dpm_client_->send_command("SetRunState", "Enable");

#endif

}

void lbne::TpcRceReceiver::stop(void)
{
	DAQLogger::LogInfo(instance_name_) << "stop() called";

#ifndef NO_RCE_CLIENT

	// Instruct the DPM to stop
	// Do this step in stopNoMutex now
	//dpm_client_->send_command("SetRunState", "Stopped");
	
	//if (dtm_client_enable_)
	//{
	  //dtm_client_->send_command("SetRunState", "Stopped");
	  //dtm_client_->send_command("Stop");

	//}
	
	//dpm_client_->send_command("STOP");
#endif

	// Stop the data receiver.
	data_receiver_->stop();

	DAQLogger::LogInfo(instance_name_) << "Low water mark on empty buffer queue is " << empty_buffer_low_mark_;
	DAQLogger::LogInfo(instance_name_) << "High water mark on filled buffer queue is " << filled_buffer_high_mark_;

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_received_) / elapsed_secs;
	double data_rate_mbs = ((double)total_bytes_received_) / ((1024*1024) * elapsed_secs);

	DAQLogger::LogInfo(instance_name_) << "Received " << millislices_received_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz, total data " << total_bytes_received_ << " bytes, rate " << data_rate_mbs << " MB/s" << std::endl;

}

void lbne::TpcRceReceiver::stopNoMutex(void)
{

	DAQLogger::LogInfo(instance_name_) << "In stopNoMutex - instructing DPM to stop";

	// Instruct the DPM to stop
	dpm_client_->send_command("SetRunState", "Stopped");
}

#ifndef OLD_GETNEXT
bool lbne::TpcRceReceiver::getNext_(artdaq::FragmentPtrs & frags) {

	uint32_t buffers_released = 0;
	RceRawBufferPtr recvd_buffer;
	const unsigned int buffer_recv_timeout_us = 500000;

	// Before releasing any fragments, capture buffer state metrics
	size_t empty_buffers_available = data_receiver_->empty_buffers_available();
	if (empty_buffers_available < empty_buffer_low_mark_)
	{
	  empty_buffer_low_mark_ = empty_buffers_available;
	}
	size_t filled_buffers_available = data_receiver_->filled_buffers_available();
	if (filled_buffers_available > filled_buffer_high_mark_)
	{
	  filled_buffer_high_mark_ = filled_buffers_available;
	}

	// JCF, Dec-11_2015

	// If it's the start of data taking, we pretend we received a
	// buffer at the start time in order to begin the clock used
	// to determine later whether or not we're timed out (this
	// code is required if no buffers ever arrive and we can't
	// therefore literally say that a buffer was received at some
	// point in time)

	if ( startOfDatataking() ) {
	  DAQLogger::LogInfo(instance_name_) << board_id() << ": start of datataking";
	  last_buffer_received_time_ = std::chrono::high_resolution_clock::now();
	}

	// If we find buffers in the while loop, then at the bottom of
	// this function we'll skip the timeout check...
	bool buffers_found_in_while_loop = false;  

	// Loop to release fragments if filled buffers available, up to the maximum allowed
	while ((data_receiver_->filled_buffers_available() > 0) && (buffers_released <= filled_buffer_release_max_))
	{
		bool buffer_available = data_receiver_->retrieve_filled_buffer(recvd_buffer, buffer_recv_timeout_us);

		if (!buffer_available)
		{
			DAQLogger::LogWarning(instance_name_) << "lbne::TpcRceReceiver::getNext_ : "
					<< "receiver indicated buffers available but unable to retrieve one within timeout";
			continue;
		}

		if (recvd_buffer->size() == 0)
		{
			DAQLogger::LogWarning(instance_name_) << "lbne::TpcRceReceiver::getNext_ : no data received in raw buffer";
			continue;
		}

		// We now know we have a legitimate buffer which we
		// can save in an artdaq::Fragment, so record the time
		// it occurred as a point of reference to later
		// determine if there's been a data timeout

		buffers_found_in_while_loop = true;
		last_buffer_received_time_ = std::chrono::high_resolution_clock::now();

		// Get a pointer to the data in the current buffer and create a new fragment pointer
		uint8_t* data_ptr = recvd_buffer->dataPtr();
		std::unique_ptr<artdaq::Fragment> frag;
		uint32_t millislice_size = 0;

		// If using fragments as raw buffers, process accordingly
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
				DAQLogger::LogError(instance_name_) << "lbne::TpcRceReceiver::getNext_ : cannot map raw buffer with data address"
						<< (void*)recvd_buffer->dataPtr() << " back onto fragment";
				continue;
			}
		}
		// Otherwise format the raw buffer into a new fragment
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

		// Recycle the raw buffer onto the commit queue for re-use by the receiver.
		data_receiver_->commit_empty_buffer(recvd_buffer);

		// Set fragment fields appropriately
		frag->setSequenceID(ev_counter());
		frag->setFragmentID(fragmentIDs()[0]);
		frag->setUserType(lbne::detail::TPC);

		// Resize fragment to final millislice size
		frag->resizeBytes(millislice_size);

		// Add the fragment to the list
		frags.emplace_back(std::move (frag));

		// Increment the event counter
		ev_counter_inc();

		// Update counters
		millislices_received_++;
		total_bytes_received_ += millislice_size;
		buffers_released++;

	} // End of loop over available buffers

	// Report on data received so far
	if (reporting_interval_time_ != 0)
	{
		std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();

		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - report_time_).count() >
			(reporting_interval_time_ * 1000))
		{
			auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
			double elapsed_secs = ((double)elapsed_msecs)/1000;

			DAQLogger::LogInfo(instance_name_) << "lbne::TpcRceReceiver::getNext_ : received " << millislices_received_ << " millislices, "
					<< float(total_bytes_received_)/(1024*1024) << " MB in " << elapsed_secs << " seconds";

			report_time_ = std::chrono::high_resolution_clock::now();
		}
	}

	// Send buffer metrics based on values captured at entry into this function

	metricMan_->sendMetric(empty_buffer_low_water_metric_name_, empty_buffer_low_mark_, "buffers", 1, false, true);
	metricMan_->sendMetric(empty_buffer_available_metric_name_, empty_buffers_available, "buffers", 1, false, true);
	metricMan_->sendMetric(filled_buffer_high_water_metric_name_, filled_buffer_high_mark_, "buffers", 1, false, true);
	metricMan_->sendMetric(filled_buffer_available_metric_name_, filled_buffers_available, "buffers", 1, false, true);

	// Determine return value, depending on whether run is stopping, if there are any filled buffers available
	// and if the receiver thread generated an exception

	bool is_active = true;
	if (should_stop())
	{
		if (data_receiver_->filled_buffers_available() > 0)
		{
			// Don't signal board reader can stop if there are still buffers available to release as fragments
			DAQLogger::LogDebug(instance_name_)
				<< "lbne::TpcRceReceiver::getNext_ : should_stop() is true but buffers available";
		}
		else
		{
			// If all buffers released, set is_active false to signal board reader can stop
			DAQLogger::LogInfo(instance_name_)
				<< "lbne::TpcRceReceiver::getNext_ : no unprocessed filled buffers available at end of run";
			is_active = false;
		}
	}
	if (data_receiver_->exception())
	{
	  set_exception(true);
	  DAQLogger::LogError(instance_name_) << "lbne::TpcRceReceiver::getNext_ : found receiver thread in exception state";
	}

	// JCF, Dec-11-2015

	// Finally, if we haven't received a buffer after a set amount
	// of time, give up (i.e., throw an exception)

	if ( ! buffers_found_in_while_loop ) {

	  auto time_since_last_buffer = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - last_buffer_received_time_).count();

	  if (time_since_last_buffer > data_timeout_usecs_) {
	    DAQLogger::LogError(instance_name_) << "lbne::TpcRceReceiver::getNext_ : timeout due to no data appearing after " << time_since_last_buffer / 1000000.0 << " seconds";
	  }
	}

	return is_active;
}
#else
bool lbne::TpcRceReceiver::getNext_(artdaq::FragmentPtrs & frags) {

  RceRawBufferPtr recvd_buffer;
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
	DAQLogger::LogInfo(instance_name_) << "Received " << millislices_received_ << " millislices so far";
      }
    }
  }
  while (!buffer_available && !should_stop() && !data_receiver_->exception());


  // If stopping (or if an exception occurred in the RceDataReceiver
  // object), check if there are any filled buffers available
  // (i.e. fragments received but not processed) and print a warning,
  // then return false to indicate no more fragments to process

  // JCF, Nov-3-2015

  // Additionally, if the RceDataReceiver object is in an exception
  // state, set the exception state for the fragment generator itself,
  // so no more attempts at collecting data will take place

  if (should_stop() || data_receiver_->exception()) {

		if( data_receiver_->filled_buffers_available() > 0)
		{
			DAQLogger::LogWarning(instance_name_) << "getNext_ stopping while there were still filled buffers available";
		}
		else
		{
	    	DAQLogger::LogInfo(instance_name_) << "No unprocessed filled buffers available at end of run";
		}

		if (data_receiver_->exception()) {
		  set_exception(true);

		  // JCF, Nov-3-2015

		  // This error message throws an exception, which
		  // means the boardreader process, when queried, will
		  // return an error state
		  
		  // JCF, Nov-4-2015

		  // Instead of throwing an exception via
		  // DAQLogger::LogError, throw an std::runtime_error,
		  // as this won't have to compete with messages on
		  // the receive thread (presumably run amok at this
		  // point)

		  throw std::runtime_error("RceDataReceiver object found in exception state");

		}

		return false;
  }


  // If there was no data received (or an error flagged), simply return
  // an empty list
  if (recvd_buffer->size() == 0)
  {
	  DAQLogger::LogWarning(instance_name_) << "getNext_ : no data received in raw buffer";
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
		  DAQLogger::LogError(instance_name_) << "Cannot map raw buffer with data address" << (void*)recvd_buffer->dataPtr() << " back onto fragment";
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

	  DAQLogger::LogInfo(instance_name_) << "Received " << millislices_received_ << " millislices, "
			  << float(total_bytes_received_)/(1024*1024) << " MB in " << elapsed_secs << " seconds";
  }

  // Update buffer high and low water marks
  size_t empty_buffers_available = data_receiver_->empty_buffers_available();
  if (empty_buffers_available < empty_buffer_low_mark_)
  {
	  empty_buffer_low_mark_ = empty_buffers_available;
  }
  size_t filled_buffers_available = data_receiver_->filled_buffers_available();
  if (filled_buffers_available > filled_buffer_high_mark_)
  {
	  filled_buffer_high_mark_ = filled_buffers_available;
  }
  metricMan_->sendMetric(empty_buffer_low_water_metric_name_, empty_buffer_low_mark_, "buffers", 1, false, true);
  metricMan_->sendMetric(empty_buffer_available_metric_name_, empty_buffers_available, "buffers", 1, false, true);
  metricMan_->sendMetric(filled_buffer_high_water_metric_name_, filled_buffer_high_mark_, "buffers", 1, false, true);
  metricMan_->sendMetric(filled_buffer_available_metric_name_, filled_buffers_available, "buffers", 1, false, true);

  // Recycle the raw buffer onto the commit queue for re-use by the receiver.
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
#endif

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
      DAQLogger::LogError(instance_name_)
        << "Unable to create microslice number " << udx;
    }
    else {
      for (uint32_t ndx = 0; ndx < number_of_nanoslices_to_generate_; ++ndx) {
        nanoslice_writer_ptr = microslice_writer_ptr->reserveNanoSlice(NANOSLICE_BUFFER_SIZE);
        if (nanoslice_writer_ptr.get() == 0) {
          DAQLogger::LogError(instance_name_)
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
	  DAQLogger::LogError(instance_name_)
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


// JCF, Dec-12-2015

// startOfDatataking() will figure out whether we've started taking
// data either by seeing that the run number's incremented since the
// last time it was called (implying the start transition's occurred)
// or the subrun number's incremented (implying the resume
// transition's occurred). On the first call to the function, the
// "last checked" values of run and subrun are set to 0, guaranteeing
// that the first call will return true

bool lbne::TpcRceReceiver::startOfDatataking() {

  static int subrun_at_last_check = 0;
  static int run_at_last_check = 0;

  bool is_start = false;

  if ( (run_number() > run_at_last_check) ||
       (subrun_number() > subrun_at_last_check) ) {
    is_start = true;
  }

  subrun_at_last_check=subrun_number();
  run_at_last_check=run_number();

  return is_start;
}


// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::TpcRceReceiver) 
