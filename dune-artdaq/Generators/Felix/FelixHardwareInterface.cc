#include "artdaq/Application/CommandableFragmentGenerator.hh"
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
  //artdaq_request_receiver_{ ps }, // This automatically setups requestReceiver!
  taking_data_(false),
  fragment_type_(dune::toFragmentType("FELIX")), 
  usecs_between_sends_(0), //ps.get<size_t>("usecs_between_sends", 0)),
  start_time_(fake_time_),
  stop_time_(fake_time_),
  send_calls_(0)
{
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Configuration for HWInterface from FHiCL.";
  fhicl::ParameterSet hps = ps.get<fhicl::ParameterSet>("HWInterface");
  fake_triggers_ = hps.get<bool>("fake_triggers");
  extract_ = hps.get<bool>("extract");
  queue_size_ = hps.get<unsigned>("queue_size");
  message_size_ = hps.get<size_t>("message_size");
  backend_ = hps.get<std::string>("backend");
  zerocopy_ = hps.get<bool>("zerocopy");
  offset_ = hps.get<unsigned>("offset");
  window_ = hps.get<unsigned>("window");

  requester_address_ = ps.get<std::string>("requester_address");
  request_address_ = ps.get<std::string>("request_address");
  request_port_ = ps.get<unsigned short>("request_port");
  requests_size_ = ps.get<unsigned short>("requests_size");

  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
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

  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up RequestReceiver.";
  request_receiver_.setup(requester_address_, request_address_, request_port_, requests_size_);

  // This is done by the RequestReceiver's constructor! Could it duplicate requests?
  //DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
  //  << "Setting up ArtDAQ's RequestReceiver.";
  //artdaq_request_receiver_.setupRequestListener();

  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up NetioHandler (host, port, adding channels, starting subscribers, locking subs to CPUs.)";
  nioh_.setupContext( backend_ ); // posix or infiniband
  for ( auto const & link : link_parameters_ ){ // Add channels
    nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
  }
  nioh_.setVerbosity(true);
  nioh_.setFrameSize( ps.get<unsigned>("frame_size") );
  nioh_.setMessageSize(message_size_);
  nioh_.setTimeWindow(window_);
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "FelixHardwareInterface is ready.";
}

FelixHardwareInterface::~FelixHardwareInterface(){
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Destructing FelixHardwareInterface. Joining request thread.";
  nioh_.stopContext();
}


void FelixHardwareInterface::StartDatataking() {
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StartDatataking") << "Start datataking...";

  // GLM: start listening to felix stream here
  nioh_.startSubscribers();
  nioh_.lockSubsToCPUs(offset_);
  nioh_.setExtract(extract_);



  taking_data_.store( true );
  send_calls_ = 0;
  fake_trigger_ = 0;
  fake_trigger_attempts_ = 0;
  nioh_.startTriggerMatchers(); // Start trigger matchers in NIOH.
  
  request_receiver_.start(); // Start request receiver.

  //artdaq_request_receiver_.ClearRequests();
  //artdaq_request_receiver_.startRequestReceiverThread(); // Start artdaq's request receiver.
  //DAQLogger::LogInfo("dune::FelixHardwareInterface::StartDatataking")
  //  << "ArtDAQ's Request listener is running? -> " << artdaq_request_receiver_.isRunning();
  start_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StartDatataking")
    << "Request listener and trigger matcher threads created.";

  //artdaq::Globals::metricMan_->sendMetric("TestMetrics", 1, "Events", 3, artdaq::MetricMode::Accumulate);

}

void FelixHardwareInterface::StopDatataking() {
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "Stop datataking...";

  // GLM: stop listening to FELIX stream here
  nioh_.stopSubscribers();

  taking_data_.store( false );

  // Request receiver stop.
  request_receiver_.stop();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "Request thread joined...";

  //std::ostringstream l1str;
  //l1str << "ARTDAQ'S L1RECEIVER STOPDATATAKING. Requests still in map: -> \n";
  //auto reqMap = artdaq_request_receiver_.GetRequests();
  //for(auto it = reqMap.cbegin(); it != reqMap.cend(); ++it) {
  //  l1str << " SEQID:" << std::hex << it->first << " TS:" << it->second << "\n";
  //}
  //DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << l1str.str(); 
  
  //artdaq_request_receiver_.ClearRequests();

  // Netio busy check.
  while (nioh_.busy()) {
    std::this_thread::sleep_for( std::chrono::microseconds(50) );
  }
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "NIOH is not busy...";

  // Stop triggermatchers.
  nioh_.stopTriggerMatchers();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "Stopped triggerMatchers..";

  // Clean stop.
  stop_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking")
    << "Datataking stopped.";
}

bool FelixHardwareInterface::FillFragment( std::unique_ptr<artdaq::Fragment>& frag , unsigned& report ){
  if (taking_data_) {
    //DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") << "Fill fragment at: " << &frag;

    // Fake trigger mode for debugging purposes. (send 10000 fake triggers.)
    if (fake_triggers_ && fake_trigger_attempts_ < 1000) {
      DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
        << " Faking a trigger -> " << fake_trigger_ << ". trigger";
      bool ok = nioh_.triggerWorkers((uint64_t)fake_trigger_, (uint64_t)fake_trigger_, frag);
      if (ok){
        DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
          << " NIOH returned OK for fake trigger.";
      } else {
        DAQLogger::LogWarning("dune::FelixHardwareInterface::FillFragment") 
          << " Couldn't fullfil fake trigger!";
      }
      fake_trigger_++;
      fake_trigger_attempts_++;
      return true; // either we hit the limit, or not taking data anymore.

    } else {
      // Check if there are requests available...
      // Safety spin to avoid waiting infinitely.
      unsigned triescount=0;
      while ( taking_data_.load() && !request_receiver_.requestAvailable() ) { // artdaq_request_receiver_.size()==0 ) { // !request_receiver_.requestAvailable() ){
        std::this_thread::sleep_for( std::chrono::microseconds(500) ); // waiting for requests to arrive...
        if (triescount>1000) {
          //DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
          //  << " Didn't receive requests for some time...\n";
	  //GLM: clean data queue a bit if it's getting full
	  nioh_.triggerWorkers(0, 0, frag);
          triescount = 0;
          report = 1;
          return false;
        } else {
          triescount++;
        }
      }

      //std::ostringstream l1str;
      //l1str << "ARTDAQ'S L1RECEIVER -> \n";
      //auto reqMap = artdaq_request_receiver_.GetRequests();
      //for(auto it = reqMap.cbegin(); it != reqMap.cend(); ++it) {
      //  l1str << " SEQID:" << std::hex << it->first << " TS:" << it->second << "\n";
      //}
      //DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") << l1str.str();

      // There are requests queued in...
      artdaq::detail::RequestPacket request;
      request_receiver_.getNextRequest( request );
      uint64_t requestSeqId = request.sequence_id;
      uint64_t requestTimestamp = request.timestamp;

      //auto reqMap = artdaq_request_receiver_.GetRequests();
      //uint64_t requestSeqId = reqMap.cbegin()->first;
      //uint64_t requestTimestamp = reqMap.cbegin()->second;

      //DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
      //    << " Requested timestamp is: " << requestTimestamp 
      //    << " requested sequence_id is: " << requestSeqId ;

      bool success = nioh_.triggerWorkers(requestTimestamp, requestSeqId, frag);
      if (success) {
        frag->setSequenceID(requestSeqId);
        frag->setTimestamp(requestTimestamp);
	if (frag->dataSizeBytes() == 0) {
	  DAQLogger::LogWarning("dune::FelixHardwareInterface::FillFragment")
	    << "Returning empty fragment for TS = " << requestTimestamp << ", seqID = " << requestSeqId;
	}  
	else {
	  DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
	    << " NIOH returned OK for trigger TS " << requestTimestamp << " and seqID " << requestSeqId <<".";	  
	}
        //artdaq_request_receiver_.RemoveRequest(requestSeqId);
      } else {
        DAQLogger::LogError("dune::FelixHardwareInterface::FillFragment") 
          << " Couldn't fullfil request for TS " << requestTimestamp << "!";
        report = 2;
        return false;
      }

      //if ( to_be_reordered_ ) {
      //  bool reordered = reorder_thread_.reorderFragment( frag );
      //  if ( reordered ) ...

      ++send_calls_;
      return true;
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
  return true; // should never reach this

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

unsigned FelixHardwareInterface::MessageSize() const {
  return message_size_;
}

unsigned FelixHardwareInterface::TriggerWindowSize() const {
  return window_;
}


