#include "lbne-artdaq/Generators/TpcRceReceiver.hh"
#include "lbne-artdaq/Overlays/MilliSliceWriter.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "lbne-artdaq/Overlays/MilliSliceFragment.hh"
#include "lbne-artdaq/Overlays/MilliSliceFragmentWriter.hh"
#include "lbne-artdaq/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq/Utilities/SimpleLookupPolicy.h"
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
  rce_client_timeout_ms_ =
	ps.get<uint32_t>("rce_client_timeout_ms", 0);

  rce_data_dest_host_ =
	ps.get<std::string>("rce_data_dest_host", "127.0.0.1");
  rce_data_dest_port_ =
	ps.get<uint16_t>("rce_data_dest_port", 8989);
  rce_data_num_frags_ =
	ps.get<uint32_t>("rce_data_num_frags", 10);
  rce_data_frag_rate_ =
	ps.get<float>("rce_data_frag_rate", 10.0);

  udp_receive_port_ =
	ps.get<uint16_t>("udp_receive_port", 9999);
  raw_buffer_size_ =
	ps.get<size_t>("raw_buffer_size", 10000);
  raw_buffer_precommit_ =
	ps.get<uint32_t>("raw_buffer_precommit", 10);

  // Create an RCE client instance
  rce_client_ = std::unique_ptr<lbne::RceClient>(new lbne::RceClient(
		  rce_client_host_addr_, rce_client_host_port_, rce_client_timeout_ms_));

  // Create a RceDataReceiver instance
  data_receiver_ = std::unique_ptr<lbne::RceDataReceiver>(new lbne::RceDataReceiver(udp_receive_port_, raw_buffer_size_));

}

void lbne::TpcRceReceiver::start(void)
{
	mf::LogDebug("TpcRceReceiver") << "start() called";

	// Pre-commit raw buffers to the data receiver object

	mf::LogDebug("TpcRceReceiver") << "Pre-committing " << raw_buffer_precommit_ << " buffers of size " << raw_buffer_size_ << " to receiver";
	for (unsigned int i = 0; i < raw_buffer_precommit_; i++)
	{
		lbne::RceRawBufferPtr raw_buffer(new RceRawBuffer(raw_buffer_size_));
		mf::LogDebug("TpcRceReceiver") << "Pre-commiting raw buffer " << i << " at address " << (void*)(raw_buffer->dataPtr());
		data_receiver_->commitEmptyBuffer(raw_buffer);
	}
	data_receiver_->start();

	// Set up parameters in RCE
	rce_client_->set_param("host",  rce_data_dest_host_, "str");
	rce_client_->set_param("port",  rce_data_dest_port_, "int");
	rce_client_->set_param("frags", rce_data_num_frags_, "int");
	rce_client_->set_param("rate",  rce_data_frag_rate_, "float");

	// Send start command to RCE
	rce_client_->send_command("START");

}

void lbne::TpcRceReceiver::stop(void)
{
	mf::LogInfo("TpcRceReceiver") << "stop() called";
	data_receiver_->stop();

	rce_client_->send_command("STOP");

}


bool lbne::TpcRceReceiver::getNext_(artdaq::FragmentPtrs & frags) {

  // returning false indicates that we are done with the events in
  // this subrun or run
  if (should_stop()) {
	return false;
  }

  // Wait to receive a raw fragment buffer from the receiver thread
  RceRawBufferPtr recvd_buffer;
  data_receiver_->retrieveFilledBuffer(recvd_buffer);

  // If there was no data recevied (or an error flagged), simply return
  // an empty list
  if (recvd_buffer->size() == 0)
  {
	  mf::LogWarning("TpcRceReceiver") << "getNext_ : no data received in raw buffer";
	  return true;
  }

  mf::LogDebug("TpcRceReceiver") << "Received buffer at address " << (void*)recvd_buffer->dataPtr() << " size " << recvd_buffer->size();

  // Create an artdaq::Fragment to format the raw data into. As a crude heuristic,
  // reserve 10 times the raw data space in the fragment for formatting overhead.
  // This needs to be done more intelligently!
  size_t fragDataSize = recvd_buffer->size() * 10;
  std::unique_ptr<artdaq::Fragment> frag = artdaq::Fragment::FragmentBytes(fragDataSize);
  frag->setSequenceID(ev_counter());
  frag->setFragmentID(fragmentIDs()[0]);
  frag->setUserType(lbne::detail::TPC);

  // Format the raw digitisations (nanoslices) in the received buffer into a millislice
  // within the data payload of the fragment
  uint32_t millislice_size = format_millislice_from_raw_buffer((uint16_t*)(recvd_buffer->dataPtr()), recvd_buffer->size(),
		  (uint8_t*)(frag->dataBeginBytes()), fragDataSize);

  // Recycle the raw buffer onto the commit queue for re-use by the receiver.
  // TODO at this point we could test the number of unused buffers on the commit queue
  // against a low water mark and create/inject ones to keep the receiver running if necessary
  data_receiver_->commitEmptyBuffer(recvd_buffer);

  // Resize fragment to final millislice size
  frag->resizeBytes(millislice_size);

  // add the fragment to the list
  frags.emplace_back(std::move (frag));

  // increment the event counter
  ev_counter_inc();

  return true;
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


// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::TpcRceReceiver) 
