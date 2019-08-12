#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/Felix/FelixHardwareInterface.hh"
#include "dune-artdaq/Generators/Felix/NetioHandler.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "fhiclcpp/ParameterSet.h"

#include <random>
#include <chrono>
#include <unistd.h>
#include <iostream>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

using namespace dune;


FelixHardwareInterface::FelixHardwareInterface(fhicl::ParameterSet const& ps) :
  nioh_{ NetioHandler::getInstance() },
  //artdaq_request_receiver_{ ps }, // This automatically setups requestReceiver!
  taking_data_(false),
  first_datataking_(true),
  fragment_type_(dune::toFragmentType("FELIX")), 
  fragment_hits_type_(dune::toFragmentType("CPUHITS")),
  fragment_hits_meta_(dune::CPUHitsFragment::VERSION),
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
  window_ = hps.get<unsigned>("trigger_matching_window_ticks");
  window_offset_ = hps.get<unsigned>("trigger_matching_offset_ticks");
  reordering_ = hps.get<bool>("reordering", false);
  compression_ = hps.get<bool>("compression", false);  
  trigger_primitive_finding_ = hps.get<bool>("trigger_primitive_finding", false);
  qat_engine_ = hps.get<int>("qat_engine", -1);  
  requester_address_ = ps.get<std::string>("zmq_fragment_connection_out");
  

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
                     linkPs.get<unsigned short>("tag"),
                     linkPs.get<fhicl::ParameterSet>("TriggerPrimitiveFinding"))
    );
  }

  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up RequestReceiver.";
  request_receiver_ = std::make_unique<RequestReceiver>(requester_address_);

  nioh_.setExtract(extract_);
  nioh_.setVerbosity(true);
  nioh_.setFrameSize( ps.get<unsigned>("frame_size") );
  nioh_.setMessageSize(message_size_);
  nioh_.setTimeWindow(window_);
  nioh_.setWindowOffset(window_offset_);

  fragment_meta_.control_word = 0xabc;
  fragment_meta_.version = 1;
  fragment_meta_.reordered = 0;
  fragment_meta_.compressed = 0;

  // Reordering
  if (reordering_) { // from config
    nioh_.doReorder(true, false);
    nioh_.doCompress(true);
    fragment_meta_.reordered = 1;
  } else {
    nioh_.doReorder(false, false);
    nioh_.doCompress(false);
    fragment_meta_.reordered = 0;
  }

  // For final setup and compression engine
  nioh_.recalculateFragmentSizes();

  // QAT compression
  if (compression_) {
    int ret = nioh_.initQAT(qat_engine_);
    DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
      << "Init QAT: " << ret;
    if (ret==0){
      fragment_meta_.compressed = 1;
    } else {
      fragment_meta_.compressed = 0;
    }
  }

  // Trigger primitive finding
  nioh_.doTPFinding(trigger_primitive_finding_);

  // metadata settings
  uint32_t framesPerMsg = message_size_/nioh_.getFrameSize(); // will be 12 for a looong time.
  fragment_meta_.num_frames = window_+(framesPerMsg*2); // + safety (should be a const?)
  fragment_meta_.num_frames = nioh_.getTimeWindowNumFrames(); // RS -> New and correct way.
  //fragment_meta_.reordered = 0;
  //fragment_meta_.compressed = 0;
  fragment_meta_.offset_frames = window_offset_; // <- ???
  fragment_meta_.window_frames = window_; // should be 6000

// RS2019 -> Adding unsub func!
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Setting up NetioHandler (host, port, adding channels, starting subscribers, locking subs to CPUs.)";
  nioh_.setupContext( backend_ ); // posix or infiniband
  for ( auto const & link : link_parameters_ ){ // Add channels
    nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_, link.tpf_params_); 
  }

  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "FelixHardwareInterface is ready.";
}

FelixHardwareInterface::~FelixHardwareInterface(){
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Destructing FelixHardwareInterface. Joining request thread.";

  nioh_.stopSubscribers();

  nioh_.stopContext();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
    << "Shutting down QAT.";
  nioh_.shutdownQAT();
}


void FelixHardwareInterface::StartDatataking() {
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StartDatataking") << "Start datataking...";


  // GLM: start listening to trigger before data stream, else data stream fills up
  taking_data_.store( true );
  send_calls_ = 0;
  fake_trigger_ = 0;
  fake_trigger_attempts_ = 0;
  nioh_.startTriggerMatchers(); // Start trigger matchers in NIOH.
  nioh_.lockTrmsToCPUs(offset_);

  request_receiver_->start(); // Start request receiver.
  sleep(1);
  // GLM: start listening to felix stream here
  nioh_.setExtract(extract_);

  for ( auto const & link : link_parameters_ ){ // Add channels
    //nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
    nioh_.subscribe(link.id_, link.tag_);
  }

  if (first_datataking_){
    DAQLogger::LogInfo("dune::FelixHardwareInterface::FelixHardwareInterface")
      << "Starting subscribers and locking to CPUs...";
    nioh_.startSubscribers();
    nioh_.lockSubsToCPUs(offset_);// This should be always after startSubs!!!
    first_datataking_.store(false);
  }

  start_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StartDatataking")
    << "Request listener and trigger matcher threads created.";
}

void FelixHardwareInterface::StopDatataking() {
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "Stop datataking...";

  taking_data_.store( false );

  // GLM: stop listening to trigger requests
  request_receiver_->stop();
  DAQLogger::LogInfo("dune::FelixHardwareInterface::StopDatataking") << "Request thread joined...";

  // (PAR 2019-03-22) This was in the destructor, but there were hangs
  // at shutdown: the subscriber thread was waiting forever in netio
  // socket recv(). Trying it here, before the unsubscribe, to see if that helps. It does not
  // nioh_.stopSubscribers();

// RS2019 -> Adding unsub func.
  for ( auto const & link : link_parameters_ ){ // Add channels
    //nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
    nioh_.unsubscribe(link.id_, link.tag_);
  }
  nioh_.flushQueues();
  // GLM: stop listening to FELIX stream here
  //nioh_.stopSubscribers();

// RS2019 -> Adding unsub func.
  for ( auto const & link : link_parameters_ ){ // Add channels
    //nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
    nioh_.unsubscribe(link.id_, link.tag_);
  }
  nioh_.flushQueues();
  // GLM: stop listening to FELIX stream here
  //nioh_.stopSubscribers();

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

bool FelixHardwareInterface::FillFragment( std::unique_ptr<artdaq::Fragment>& frag, std::unique_ptr<artdaq::Fragment>& fraghits ){
  if (taking_data_) {
    //DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") << "Fill fragment at: " << &frag;

// RS: 17.08.2018 -> This will be gone for good soon.
    // Fake trigger mode for debugging purposes. (send 10000 fake triggers.)
/*
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

    } 
    else {
*/
      TriggerInfo request = request_receiver_->getNextRequest();
      uint64_t requestSeqId = request.seqID;
      uint64_t requestTimestamp = request.timestamp;

      //Compare the TPHits TS with the current Felix TS (50MHz ticks)
      std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

      //auto reqMap = artdaq_request_receiver_.GetRequests();
      //uint64_t requestSeqId = reqMap.cbegin()->first;
      //uint64_t requestTimestamp = reqMap.cbegin()->second;

      bool success = nioh_.triggerWorkers(requestTimestamp, requestSeqId, frag, fraghits);
      if (success) {
	//number of ticks per second for a 50MHz clock
	auto ticks = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1,50000000>>>(now.time_since_epoch());

        frag->setSequenceID(requestSeqId);
        frag->setTimestamp(requestTimestamp);
        frag->updateMetadata(fragment_meta_);

        fraghits->setSequenceID(requestSeqId);
        fraghits->setTimestamp(requestTimestamp);
        fraghits->updateMetadata(fragment_hits_meta_);

	if (frag->dataSizeBytes() == 0) {
	  DAQLogger::LogWarning("dune::FelixHardwareInterface::FillFragment")
	    << "Returning empty fragment for TS = " << requestTimestamp << ", seqID = " << requestSeqId;
	}
	else {
          std::ostringstream oss;
          frag->print(oss); 
	  DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") 
	    << " NIOH returned OK for trigger TS " << requestTimestamp << " and seqID " << requestSeqId <<"."
            << " Size:" << frag->dataSizeBytes() << " Brief:" << oss.str(); 
	  DAQLogger::LogInfo("dune::FelixHardwareInterface::FillFragment") << "Difference between current TS and SWTrigger request TS "
									   << ticks.count() - requestTimestamp;
	}
        //artdaq_request_receiver_.RemoveRequest(requestSeqId);
      } else {
        return false;
      }

      ++send_calls_;
      return true;

    //} EOF fake trigger check.

  } // EOF if(takingData_)

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

unsigned FelixHardwareInterface::TriggerWindowOffset() const {
  return window_offset_;
}


