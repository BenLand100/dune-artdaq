#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/Felix/FelixHardwareInterface.hh"
#include "dune-artdaq/Generators/Felix/NetioHandler.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "fhiclcpp/ParameterSet.h"

#include <random>
#include <unistd.h>
#include <iostream>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

using namespace dune;

FelixHardwareInterface::FelixHardwareInterface(fhicl::ParameterSet const& ps) :
  nioh_{ NetioHandler::getInstance() },
  taking_data_(false),
  fragment_type_(dune::toFragmentType("FELIX")), 
  usecs_between_sends_(0), //ps.get<size_t>("usecs_between_sends", 0)),
  start_time_(fake_time_),
  stop_time_(fake_time_),
  send_calls_(0)
{
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Configuration for HWInterface from FHiCL.";
  fhicl::ParameterSet hps = ps.get<fhicl::ParameterSet>("HWInterface");
  fake_triggers_ = hps.get<bool>("fake_triggers");
  message_size_ = hps.get<size_t>("message_size");
  backend_ = hps.get<std::string>("backend");
  offset_ = hps.get<unsigned>("offset");
  window_ = hps.get<unsigned>("window");
  requester_address_ = hps.get<std::string>("requester_address");
  multicast_address_ = hps.get<std::string>("multicast_address");
  multicast_port_ = hps.get<unsigned short>("multicast_port");
  requests_size_ = hps.get<unsigned short>("requests_size");

  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Configuration for Links from FHiCL."; 
  fhicl::ParameterSet lps = ps.get<fhicl::ParameterSet>("Links");
  std::vector<std::string> links = lps.get_names();
  for ( auto const & linkName : links ){
    fhicl::ParameterSet const& linkPs = lps.get<fhicl::ParameterSet>(linkName);
    link_parameters_.emplace_back(
      LinkParameters(linkPs.get<unsigned short>("id"),
                     linkPs.get<std::string>("host"),
                     linkPs.get<unsigned short>("port"),
                     linkPs.get<unsigned short>("tag"))
    );
  }

  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up RequestReceiver.";
  request_receiver_.setup(requester_address_, multicast_address_, multicast_port_, requests_size_);

  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up NetioHandler (host, port, adding channels, starting subscribers, locking subs to CPUs.)";
  nioh_.setupContext( backend_ ); // posix or infiniband
  for ( auto const & link : link_parameters_ ){ // Add channels
    nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, 3000000); 
  }
  nioh_.setVerbosity(true);
  nioh_.setMessageSize(message_size_);
  nioh_.setTimeWindow(window_);
  nioh_.startSubscribers();
  nioh_.lockSubsToCPUs();
  nioh_.setExtract(!fake_triggers_);
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "FelixHardwareInterface is ready.";
}

FelixHardwareInterface::~FelixHardwareInterface(){
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Destructing FelixHardwareInterface. Joining request thread.";
  nioh_.stopSubscribers();
  nioh_.stopContext();
}


void FelixHardwareInterface::StartDatataking() {
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StartDatataking") << "Start datataking...";
  taking_data_.store( true );
  send_calls_ = 0;
  fake_trigger_ = 0;
  fake_trigger_attempts_ = 0;
  nioh_.startTriggerMatchers(); // Start trigger matchers in NIOH.
  request_receiver_.start(); // Start request receiver.
  start_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StartDatataking")
    << "Request listener and trigger matcher threads created.";
}

void FelixHardwareInterface::StopDatataking() {
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StopDatataking") << "Stop datataking...";
  taking_data_.store( false );

  // Request receiver stop.
  request_receiver_.stop();
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StopDatataking") << "Request thread joined...";

  // Netio busy check.
  while (nioh_.busy()) {
    std::this_thread::sleep_for( std::chrono::microseconds(50) );
  }
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StopDatataking") << "NIOH is not busy...";

  // Stop triggermatchers.
  nioh_.stopTriggerMatchers();
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StopDatataking") << "Stopped triggerMatchers..";

  // Clean stop.
  stop_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::StopDatataking")
    << "Datataking stopped.";
}

void FelixHardwareInterface::FillFragment( std::unique_ptr<artdaq::Fragment>& frag ){
  if (taking_data_) {
    DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") << "Fill fragment at: " << &frag;

    // Fake trigger mode for debugging purposes. (send 10000 fake triggers.)
    if (fake_triggers_ && fake_trigger_attempts_ < 1000) {
      DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
        << " Faking a trigger -> " << fake_trigger_ << ". trigger";
      bool ok = nioh_.triggerWorkers(fake_trigger_, fake_trigger_, frag);
      if (ok){
        DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
          << " NIOH returned OK for fake trigger.";
      } else {
        DAQLogger::LogWarning("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
          << " Couldn't fullfil fake trigger!";
      }
      fake_trigger_++;
      fake_trigger_attempts_++;
      return; // either we hit the limit, or not taking data anymore.

    } else {
      // Check if there are requests available...
      while ( !request_receiver_.requestAvailable() && taking_data_.load() ){
        std::this_thread::sleep_for( std::chrono::microseconds(50) ); // waiting for requests to arrive...
      }
      // There are requests queued in...
      artdaq::detail::RequestPacket request;
      request_receiver_.getNextRequest( request );
      DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
          << " Requested timestamp is: 0x" << std::hex << request.timestamp << std::dec
          << " reuqested sequence_id is: 0x" << std::hex << request.sequence_id << std::dec;

      bool success = nioh_.triggerWorkers(request.timestamp, request.sequence_id, frag);
      if (success) {
        DAQLogger::LogInfo("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
          << " NIOH returned OK for trigger.";
      } else {
        DAQLogger::LogWarning("FelixHardwareInterface dune::FelixHardwareInterface::FillFragment") 
          << " Couldn't fullfil request!";
      }
      ++send_calls_;
      return;
    }  

/*
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
        auto usecs_since_start =
      std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now() - start_time_).count();

        long delta = (long)(usecs_between_sends_*send_calls_) - usecs_since_start;
        TRACE( 15, "FelixHardwareInterface::FillBuffer send_calls=%d usecs_since_start=%ld delta=%ld"
              , send_calls_, usecs_since_start, delta );
    if (delta > 0)
                usleep( delta );

#pragma GCC diagnostic pop
*/
  }

}

// Pretend that the "BoardType" is some vendor-defined integer which
// differs from the fragment_type_ we want to use as developers (and
// which must be between 1 and 224, inclusive) so add an offset

int FelixHardwareInterface::BoardType() const {
  return static_cast<int>(fragment_type_) + 1000; // Hardcoded fragment type.
}

int FelixHardwareInterface::SerialNumber() const {
  return 999;
}

