#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/Felix/FelixOnHostInterface.hh"
#include "dune-artdaq/Generators/Felix/FelixOnHostInterface.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "fhiclcpp/ParameterSet.h"

#include <random>
#include <chrono>
#include <unistd.h>
#include <iostream>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

using namespace dune;


FelixOnHostInterface::FelixOnHostInterface(fhicl::ParameterSet const& ps) :
  cpu_pin_{ CPUPin::getInstance() },
  queh_{ QueueHandler::getInstance() },
  flx_queue_size_{ 500000 },
//  num_sources_{ 2 },
  num_links_{ 5 },
//  card_offset_{ 0 },
  felix_id_{ 712 },

//  nioh_{ FelixOnHostInterface::getInstance() },
  //artdaq_request_receiver_{ ps }, // This automatically setups requestReceiver!
  taking_data_(false),
  first_datataking_(true),
  fragment_type_(dune::toFragmentType("FELIX")), 
  usecs_between_sends_(0), //ps.get<size_t>("usecs_between_sends", 0)),
  start_time_(fake_time_),
  stop_time_(fake_time_),
  send_calls_(0)
{
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Configuration for HWInterface from FHiCL."; 

  fhicl::ParameterSet hps = ps.get<fhicl::ParameterSet>("HWInterface");
  pin_file_ = hps.get<std::string>("pin_file");
  cpu_pin_.load(pin_file_);

  num_sources_ = hps.get<unsigned>("num_logical_units", 2);
  card_offset_ = hps.get<unsigned>("card_offset", 0);
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Number of sources:" << num_sources_ << " card offset:" << card_offset_; 

  fake_triggers_ = hps.get<bool>("fake_triggers");
  extract_ = hps.get<bool>("extract");
  queue_size_ = hps.get<unsigned>("queue_size");
  message_size_ = hps.get<size_t>("message_size");
  frame_size_ = ps.get<unsigned>("frame_size");
  backend_ = hps.get<std::string>("backend");
  zerocopy_ = hps.get<bool>("zerocopy");
  offset_ = hps.get<unsigned>("offset");
  window_ = hps.get<unsigned>("trigger_matching_window_ticks");
  window_offset_ = hps.get<unsigned>("trigger_matching_offset_ticks");
  reordering_ = hps.get<bool>("reordering", false);
  compression_ = hps.get<bool>("compression", false);  
  qat_engine_ = hps.get<int>("qat_engine", -1);  
  requester_address_ = ps.get<std::string>("zmq_fragment_connection_out");
  

  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
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

  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Setting up RequestReceiver.";
  request_receiver_ = std::make_unique<RequestReceiver>(requester_address_);

/////////////// ON HOST BR /////////////////////////////

  // PREPARE SPSC QUEUES FOR DATA
  unsigned sumNumLinks = num_sources_*num_links_;
  for (unsigned i=0; i<sumNumLinks; ++i){
    //unsigned qId = i;
    //if ( num_logical_units_ == 1 && card_offset_ == 1) { 
    //  qId+= num_links_; 
    //}
    queh_.addQueue(i, flx_queue_size_);
    last_tss_.push_back(0);
    frag_ptrs_.push_back(nullptr);
  }
 
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Allocating SPSC queues.";
  queh_.allocateQueues();
   
  // CREATE PARSERS
  for (unsigned i = 0; i<num_sources_; ++i) {
    uint32_t cno = i+card_offset_;
    for ( unsigned linkId=0; linkId<num_links_; ++linkId ){
      uint32_t tag = linkId*64+cno*1024;
      link_info_[cno][linkId] = tag;
      std::cout << "Linking QUEUE FOR " << tag << '\n';
    }
    card_readers_[cno] = std::make_unique<ProtoDuneReader>(cno, 0, link_info_[cno]);
  }
  // SPAWN PARSERS
  for (unsigned r=0; r<card_readers_.size(); ++r) {
    card_readers_[r+card_offset_]->startRunning();
    parser_threads_[r+card_offset_] = std::thread(&ProtoDuneReader::run, card_readers_[r+card_offset_].get());
    cpu_set_t cpuset = cpu_pin_.getReaderCPUs(felix_id_, r);
    pthread_setaffinity_np(parser_threads_[r+card_offset_].native_handle(), sizeof(cpu_set_t), &cpuset);
  }
  
/*
  nioh_.setExtract(extract_);
  nioh_.setVerbosity(true);
  nioh_.setFrameSize( ps.get<unsigned>("frame_size") );
  nioh_.setMessageSize(message_size_);
  nioh_.setTimeWindow(window_);
  nioh_.setWindowOffset(window_offset_);
*/

  fragment_meta_.control_word = 0xabc;
  fragment_meta_.version = 1;
  fragment_meta_.reordered = 0;
  fragment_meta_.compressed = 0;

  m_reorderFacility = std::make_unique<ReorderFacility>(false); // forceNoAVX = false
  // Reordering
  if (reordering_) { // from config
    m_compressionFacility = std::make_unique<QzCompressor>(QzCompressor::QzAlgo::Deflate, 4, 64);
    fragment_meta_.reordered = 1;
  } else {
    fragment_meta_.reordered = 0;
  }

  // For final setup and compression engine
  RecalculateFragmentSizes();

  // QAT compression
  if (compression_) {
    int ret = m_compressionFacility->init(m_timeWindowByteSizeOut, -1); // engine
    DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
      << "Init QAT: " << ret;
    if (ret==0){
      fragment_meta_.compressed = 1;
    } else {
      compression_ = false;
      fragment_meta_.compressed = 0;
    }
  }


  // metadata settings
  uint32_t framesPerMsg = message_size_/frame_size_; //nioh_.getFrameSize(); // will be 12 for a looong time.
  fragment_meta_.num_frames = window_+(framesPerMsg*2); // + safety (should be a const?)
  fragment_meta_.num_frames = m_timeWindowNumFrames; //nioh_.getTimeWindowNumFrames(); // RS -> New and correct way.
  //fragment_meta_.reordered = 0;
  //fragment_meta_.compressed = 0;
  fragment_meta_.offset_frames = window_offset_; // <- ???
  fragment_meta_.window_frames = window_; // should be 6000

// RS2019 -> Adding unsub func!

/*
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Setting up FelixOnHostInterface (host, port, adding channels, starting subscribers, locking subs to CPUs.)";
  nioh_.setupContext( backend_ ); // posix or infiniband
  for ( auto const & link : link_parameters_ ){ // Add channels
    nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
  }
*/

  SetupTriggerMatchers();
  LockTriggerMatchers();

  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "FelixOnHostInterface is ready.";
}

FelixOnHostInterface::~FelixOnHostInterface(){
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Destructing FelixOnHostInterface. Joining request thread.";
// RS2019 -> Adding unsub func! Probably this is at a VERY VERY WRONG PLACE.

  m_compressionFacility->shutdown();

/*
  nioh_.stopSubscribers();

  nioh_.stopContext();
*/
  DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
    << "Shutting down QAT.";
//  nioh_.shutdownQAT();
}




void FelixOnHostInterface::StartDatataking() {
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StartDatataking") << "Start datataking...";

// RS2019 -> Adding unsub func!
  //DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
  //  << "Setting up FelixOnHostInterface (host, port, adding channels, starting subscribers, locking subs to CPUs.)";
  //nioh_.setupContext( backend_ ); // posix or infiniband

  // GLM: start listening to trigger before data stream, else data stream fills up
  taking_data_.store( true );
  send_calls_ = 0;
  fake_trigger_ = 0;
  fake_trigger_attempts_ = 0;


//  nioh_.startTriggerMatchers(); // Start trigger matchers in NIOH.
//  nioh_.lockTrmsToCPUs(offset_);

//  SetupTriggerMatchers();
//  LockTriggerMatchers();

  request_receiver_->start(); // Start request receiver.
  sleep(1);
  // GLM: start listening to felix stream here
//  nioh_.setExtract(extract_);

//  for ( auto const & link : link_parameters_ ){ // Add channels
//    //nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
//    nioh_.subscribe(link.id_, link.tag_);
//  }

  if (first_datataking_){
    DAQLogger::LogInfo("dune::FelixOnHostInterface::FelixOnHostInterface")
      << "Starting subscribers and locking to CPUs...";
//    nioh_.startSubscribers();
//    nioh_.lockSubsToCPUs(offset_);// This should be always after startSubs!!!
    first_datataking_.store(false);
  }

  start_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StartDatataking")
    << "Request listener and trigger matcher threads created.";
}

void FelixOnHostInterface::StopDatataking() {
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StopDatataking") << "Stop datataking...";

  taking_data_.store( false );

  // GLM: stop listening to trigger requests
  request_receiver_->stop();
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StopDatataking") << "Request thread joined...";


// RS2019 -> Adding unsub func.
//  for ( auto const & link : link_parameters_ ){ // Add channels
//    //nioh_.addChannel(link.id_, link.tag_, link.host_, link.port_, queue_size_, zerocopy_); 
//    nioh_.unsubscribe(link.id_, link.tag_);
//  }
//  nioh_.flushQueues();
  // GLM: stop listening to FELIX stream here
  //nioh_.stopSubscribers();

  // Netio busy check.
//  while (nioh_.busy()) {
//    std::this_thread::sleep_for( std::chrono::microseconds(50) );
//  }
//  DAQLogger::LogInfo("dune::FelixOnHostInterface::StopDatataking") << "NIOH is not busy...";

  // Stop triggermatchers.
//  nioh_.stopTriggerMatchers();
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StopDatataking") << "Stopped triggerMatchers..";

  // Clean stop.
  stop_time_ = std::chrono::high_resolution_clock::now();
  DAQLogger::LogInfo("dune::FelixOnHostInterface::StopDatataking")
    << "Datataking stopped.";
}

bool FelixOnHostInterface::FillFragments( artdaq::FragmentPtrs& frags ) {
  if (taking_data_) {
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
    TriggerInfo request = request_receiver_->getNextRequest();
    uint64_t requestSeqId = request.seqID;
    uint64_t requestTimestamp = request.timestamp;
    trigger_ts_ = requestTimestamp;
    trigger_seq_id_ = requestSeqId;
    auto tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
    DAQLogger::LogInfo("FelixOnHostInterface::FillFragments") << " Getting a new request took " << tdelta.count() << " us";

    t1 = std::chrono::high_resolution_clock::now();
    bool success = TriggerWorkers(frags);
    tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
    DAQLogger::LogInfo("FelixOnHostInterface::FillFragments") << " TriggerWorkers took " << tdelta.count() << " us";

    if (success) {
//      frag[0]->setSequenceID(requestSeqId);
//      frag[0]->setTimestamp(requestTimestamp);
//      frag[0]->updateMetadata(fragment_meta_);      
    }

   // DAQLogger::LogWarning("dune::FelixOnHostInterface::FillFragment")
   //   << "Returning empty fragment for TS = " << requestTimestamp << ", seqID = " << requestSeqId; 
    ++send_calls_;
    return true;
  }
  return true;
}

bool FelixOnHostInterface::Busy(){
  uint32_t busyLinks = 0;
  for (uint32_t i=0; i<trm_extractors_.size(); ++i) {
    bool busy = (trm_extractors_[i]->get_readiness()==false) ? true : false;
    if (busy) { busyLinks++; }
  }
  return (busyLinks != 0) ? true : false;
}


bool FelixOnHostInterface::TriggerWorkers(artdaq::FragmentPtrs& frags) {
  if (stop_trigger_.load()) return false;

  std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
  uint32_t i = 0;
  for (auto&& frag : frags){
    frag_ptrs_[i] = frag.get();
    trm_extractors_[i]->set_work(trm_functors_[i]);
    ++i;
  }
  auto tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
  DAQLogger::LogInfo("FelixOnHostInterface::TriggerWorkers") << " Setting work took " << tdelta.count() << " us";

  //for (uint32_t i=0; i<queh_.getNumOfChannels(); ++i) {
  //  frag_ptrs_[i] = frags[i].get();
  //  trm_extractors_[i]->set_work(trm_functors_[i]);  
  //}

  t1 = std::chrono::high_resolution_clock::now();
  while (Busy()) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
  DAQLogger::LogInfo("FelixOnHostInterface::TriggerWorkers") << " Busy took " << tdelta.count() << " us";

  if (trigger_ts_ == 0) {
    return false;  
  }

  return true;
}

bool FelixOnHostInterface::SetupTriggerMatchers(){
  stop_trigger_.store(false);
  for (uint32_t i=0; i<queh_.getNumOfChannels(); ++i){
    // Prepare resources
    last_tss_.push_back(0);
    trm_extractors_.emplace_back( new ReusableThread(i) );
    set_thread_name(trm_extractors_[i]->get_thread(), "trm", i);
    uint32_t tid = i;
    uint32_t framesPerMsg = message_size_/frame_size_;
    size_t frameCapacity = flx_queue_size_;

    // Create functions
    trm_functors_.push_back( [&, tid, frameCapacity, framesPerMsg] {
      frag_ptrs_[tid]->setSequenceID(trigger_seq_id_);
      frag_ptrs_[tid]->setTimestamp(trigger_ts_);
      frag_ptrs_[tid]->updateMetadata(fragment_meta_);

      UniqueFrameQueue& queue = queh_.getQueue(tid);
      if(stop_trigger_.load()){ // Safety if stop trigger is issued.
        DAQLogger::LogInfo("FelixOnHostInterface::SetupTriggerMatchers")
          << "Should stop triggers, bailing out.\n";
          return;
      }

      if (queue->isEmpty()) {
        DAQLogger::LogWarning("FelixOnHostInterface::TriggerMatcher")
          << " ["<< tid << "] Queue is empty... Is FelixCore publishing data?\n";
        return;
      }

      if (trigger_ts_==0){ // Check if we got a trigger request
        size_t qsize = queue->sizeGuess();
        if (qsize > 0.5 * flx_queue_size_){
          DAQLogger::LogInfo("FelixOnHostInterface::TriggerMatcher") 
            << " ["<< tid <<"] Removing old non requested data; (" << qsize << ") messages in queue.";
          queue->popXFront(0.8 * qsize);
        }
        return;
      }
      // From here on we got a trigger and we need to extract.
      uint_fast64_t startWindowTimestamp = trigger_ts_ - (uint_fast64_t)(window_offset_ * tick_dist_);
      dune::WIBHeader wh = *(reinterpret_cast<const dune::WIBHeader*>( queue->frontPtr() ));
      last_tss_[tid] = wh.timestamp();
      uint_fast64_t lastTs = last_tss_[tid];
      // Check if there is such a delay in trigger requests that data have gone already.
      if (lastTs > startWindowTimestamp ) {
        DAQLogger::LogWarning("FelixOnHostInterface::TriggerMatcher")
          << "Requested data are so old that they were dropped. Trigger request TS = "
         << trigger_ts_ << ", oldest TS in queue = "  << lastTs;
        return;
      }

      // Calculate tick difference
      uint_fast64_t timeTickDiff = (startWindowTimestamp-lastTs)/(uint_fast64_t)tick_dist_;

      // Wait to have enough frames in the queue
      uint_fast64_t minQueueSize = (timeTickDiff + window_) / framesPerMsg + 10;
      size_t qsize = queue->sizeGuess();
      uint_fast32_t waitingForDataCtr = 0;
      while (qsize < minQueueSize){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        qsize = queue->sizeGuess();
        ++waitingForDataCtr;
        if (waitingForDataCtr > 20000){
          DAQLogger::LogWarning("FelixOnHostInterface::TriggerMatcher")
            << "Data stream delayed by over 2 secs with respect to trigger requests! ";
          return;
        }
      }

      // Pop until the start of window.
      queue->popXFront(timeTickDiff/framesPerMsg);

      // FILL FRAGMENT at frag_ptrs[tid]
      frag_ptrs_[tid]->resizeBytes(m_timeWindowByteSizeOut);
      uint64_t fragSize = m_timeWindowByteSizeOut;
      if (!reordering_)
      {
        for (unsigned i = 0; i < m_timeWindowNumMessages; i++)
        {
          memcpy(frag_ptrs_[tid]->dataBeginBytes() + message_size_ * i, (char *)queue->frontPtr(), message_size_);
          queue->popFront();
        } 
      } else {
        m_reorderFacility->do_reorder_start(m_timeWindowNumFrames);
        for (unsigned i = 0; i < m_timeWindowNumMessages; i++) {
          m_reorderFacility->do_reorder_part(
            frag_ptrs_[tid]->dataBeginBytes(),
            (uint_fast8_t *)queue->frontPtr(),
            i * framesPerMsg, (i + 1) * framesPerMsg
          );
          queue->popFront();
        }
      }

      if (compression_) {
        uint_fast32_t compSize = m_compressionFacility->do_compress(frag_ptrs_[tid], fragSize);
        DAQLogger::LogInfo("FelixOnHostInterface::TriggerMatcher") << "[" << tid << "] Compressed size: " << compSize;
      } else {
        if (reordering_) {
          frag_ptrs_[tid]->resizeBytes(fragSize);
        }
      }

      std::ostringstream oss;
      frag_ptrs_[tid]->print(oss);
      DAQLogger::LogInfo("FelixOnHostInterface::TriggerMatcher") << "[" << tid << "] Created frag: " << oss.str();

    });
  }
  return true; 
}

void FelixOnHostInterface::LockTriggerMatchers() {
  DAQLogger::LogInfo("FelixOnHostInterface::LockTriggerMatchers") << "Attempt to lock TriggerMatchers to CPUs.";

  //unsigned short cpuid = 0; // Beautiful hardcode.... :)
  for (unsigned i=0; i< trm_extractors_.size(); ++i) { //  set_thread_name(m_extractors[i]->get_thread(), "nioh-trm", i); 
    //CPU_SET(cpuid, &cpuset);
    // CPU_SET(cpuid+24, &cpuset);
    // RS : Not a good attempt.
    //CPU_SET(cpuid+16, &cpuset);

    cpu_set_t cpuset = cpu_pin_.getSelectorCPUs(i);
    int ret = pthread_setaffinity_np(trm_extractors_[i]->get_thread().native_handle(), sizeof(cpu_set_t), &cpuset);
    if (ret!=0) {
      DAQLogger::LogError("FelixOnHostInterface::lockTrmsToCPUs")
        << "Error calling pthread_setaffinity! Return code:" << ret;
    }
  }
  DAQLogger::LogInfo("FelixOnHostInterface::LockTriggerMatchers") << "Trigger matchers locked!";
}

void FelixOnHostInterface::RecalculateFragmentSizes()
{
    DAQLogger::LogInfo("FelixOnHostInterface::RecalculateFragmentSizes") << "Recalculating...";
    size_t framesPerMsg = message_size_ / frame_size_;
    m_timeWindowNumMessages = (window_ / framesPerMsg) + 2;

    m_timeWindowByteSizeIn = message_size_ * m_timeWindowNumMessages;
    m_timeWindowNumFrames = m_timeWindowByteSizeIn / FelixReorder::m_num_bytes_per_frame;

    if (reordering_)
    {
      m_timeWindowByteSizeOut = m_timeWindowByteSizeIn * FelixReorder::m_num_bytes_per_reord_frame / FelixReorder::m_num_bytes_per_frame;
      // We need to account for the added bitfield to keep track of which headers are saved
      // This size is also not the actual size, but the maximum size
      // Number of bits needed is rounded up to bytes.
      m_timeWindowByteSizeOut += (m_timeWindowNumFrames + 7) / 8;
    }
    else
    {
      m_timeWindowByteSizeOut = m_timeWindowByteSizeIn;
    }
    DAQLogger::LogInfo("FelixOnHostInterface::recalculateFragmentSizes")
      << "Recalculated sizes: framesPerMsg:" << framesPerMsg
      << " | messages per timewindow: " << m_timeWindowNumMessages
      << " | WIB frames per timewindow:" << m_timeWindowNumFrames
      << " | original fragment size: " << m_timeWindowByteSizeIn
      << " | reordered fragment size: " << m_timeWindowByteSizeOut
      << " | reorder info: " << m_reorderFacility->get_info();
}


// Pretend that the "BoardType" is some vendor-defined integer which
// differs from the fragment_type_ we want to use as developers (and
// which must be between 1 and 224, inclusive) so add an offset

int FelixOnHostInterface::BoardType() const {
  return static_cast<int>(fragment_type_) + 1000; // Hardcoded fragment type.
}

int FelixOnHostInterface::SerialNumber() const {
  return 999;
}

unsigned FelixOnHostInterface::MessageSize() const {
  return message_size_;
}

unsigned FelixOnHostInterface::TriggerWindowSize() const {
  return window_;
}

unsigned FelixOnHostInterface::TriggerWindowOffset() const {
  return window_offset_;
}


