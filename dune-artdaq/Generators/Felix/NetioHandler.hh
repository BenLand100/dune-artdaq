#ifndef NETIO_HANDLER_HH_
#define NETIO_HANDLER_HH_

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "ProducerConsumerQueue.hh"
#include "ReusableThread.hh"
#include "NetioWIBRecords.hh"
#include "Utilities.hh"
#include "Types.hh"

#include "ReorderFacility.hh"
#include "QzCompressor.hh"

#include "netio/netio.hpp"

#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>

#include "TriggerPrimitive/TriggerPrimitiveFinder.h"

//#define MSGQ
//#define QACHECK

//#define REORD_DEBUG

/*
 * NetioHandler
 * Author: Roland.Sipos@cern.ch
 * Description: Wrapper class for NETIO sockets and folly SPSC circular buffers.
 *   Makes the communication with the FELIX easier and scalable.
 * Date: November 2017
*/
class NetioHandler
{
public:
  // Singleton
  static NetioHandler& getInstance(){
    static NetioHandler myInstance;
    return myInstance;
  }

  // Prevent copying and moving.
  NetioHandler(NetioHandler const&) = delete;             // Copy construct
  NetioHandler(NetioHandler&&) = delete;                  // Move construct
  NetioHandler& operator=(NetioHandler const&) = delete;  // Copy assign
  NetioHandler& operator=(NetioHandler &&) = delete;      // Move assign 

  // Custom types -> moved to Types.hh
  //typedef folly::ProducerConsumerQueue<IOVEC_CHAR_STRUCT> FrameQueue;

  void setFrameSize(size_t frameSize) { m_framesize = frameSize; }
  void setTimeWindow(size_t timeWindow) { m_timeWindow = timeWindow; }
  void setWindowOffset(size_t windowOffset) { m_windowOffset = windowOffset; }
  void setMessageSize(size_t messageSize) { m_msgsize = messageSize; }
  void setExtract(bool extract) { m_extract = extract; }
  void setVerbosity(bool v){ m_verbose = v; }

  uint32_t getMessageSize() { return m_msgsize; }
  uint32_t getFrameSize() { return m_framesize; }

  // Compression and reordering
  void doReorder(bool doIt, bool forceNoAVX) {
    m_doReorder = doIt;
    m_reorderFacility = std::make_unique<ReorderFacility>(forceNoAVX);
  }
  void doCompress(bool doIt) {
    m_doCompress = doIt;
    // QzCompressor qzc(QzCompressor::QzAlgo::Deflate, compression_level, hw_bufsize_kb);
    m_compressionFacility = std::make_unique<QzCompressor>(QzCompressor::QzAlgo::Deflate, 4, 64);
  }
  int  initQAT(int engine) {
    // If this fails, doCompression will fall back to 0.
    int ret = m_compressionFacility->init(m_timeWindowByteSizeOut, engine);
    if (ret!=0) { m_doCompress=false; }
    return ret;
  }
  void doTPFinding(bool doIt) { m_doTPFinding=doIt; }
  void shutdownQAT() { m_compressionFacility->shutdown(); }
  void recalculateByteSizes();
  void recalculateFragmentSizes();
  size_t getTimeWindowNumFrames() { return m_timeWindowNumFrames; }

  // Functionalities
  // Setup context:
  bool setupContext(std::string contextStr);
  bool stopContext();
  // Enable an elink (prepare a queue, socket-pairs and sub to elink.
  bool addChannel(uint64_t chn, uint16_t tag, std::string host, uint16_t port, size_t queueSize, bool zerocopy, fhicl::ParameterSet const& tpf_params);   
  bool subscribe(uint64_t chn, uint16_t tag); // Subscribe to given tag for elink/channel.
  bool unsubscribe(uint64_t chn, uint16_t tag); // Unsubscribe from a tag for elinkg/channel.
  bool busy(); // are trigger matchers busy
  void startTriggerMatchers(); // Starts trigger matcher threads.
  void stopTriggerMatchers();  // Stops trigger matcher threads.
  void startSubscribers(); // Starts the subscriber threads.
  void stopSubscribers();  // Stops the subscriber threads.
  void lockSubsToCPUs(uint32_t offset); // Lock subscriber threads to CPUset.
  void lockTrmsToCPUs(uint32_t offset); // Lock triggerMatcher threads to CPUset: highly experimental. :)
 
  // ArtDAQ specific
  //void setReadoutBuffer(char* buffPtr, size_t* bytePtr) { m_bufferPtr=&buffPtr; m_bytesReadPtr=&bytePtr; };
  bool triggerWorkers(uint64_t timestamp, uint64_t sequence_id,
                      std::unique_ptr<artdaq::Fragment>& frag,
                      std::unique_ptr<artdaq::Fragment>& fraghits);

  bool flushQueues();

  // Queue utils if needed
  size_t getNumOfChannels() { return m_activeChannels; } // Get the number of active channels.

protected:
  // Singleton mode
  NetioHandler(std::string contextStr, std::string felixHost, 
               uint16_t felixTXPort, uint16_t felixRXPort,
               size_t queueSize, bool verbose); 
  NetioHandler();
  ~NetioHandler();

private:
  // Consts
  const uint32_t m_headersize = sizeof(FromFELIXHeader);
  const uint_fast64_t m_tickdist=25; 

  // NETIO
  std::string m_host;
  uint16_t m_port;
  netio::context * m_context; // context
  std::thread m_netio_bg_thread; 
  std::map<uint64_t, netio::subscribe_socket*> m_sub_sockets; // subscribe sockets.
  size_t m_activeChannels;

  //Configuration:
  std::vector<uint64_t> m_channels;
  std::atomic<unsigned long> m_nmessages;
  size_t m_framesize;
  size_t m_msgsize;
  size_t m_timeWindow;
  size_t m_windowOffset;
  bool m_doReorder;
  bool m_doCompress;
  bool m_doTPFinding;
  bool m_qatReady;
  bool m_extract;
  bool m_verbose;

  //Precomputed bytesizes - Thijs
  size_t m_timeWindowByteSizeIn;
  size_t m_timeWindowByteSizeOut;
  size_t m_timeWindowNumFrames;
  size_t m_timeWindowNumMessages;

  // Queues 
#ifdef MSGQ
  std::map<uint64_t, UniqueMessageQueue> m_pcqs; // Queues for elink RX.
#else 
  std::map<uint64_t, UniqueFrameQueue> m_pcqs;
#endif

  // Map from data timestamp to the system time when we received it,
  // so we can measure the selection request latency in the software trigger
  typedef folly::ProducerConsumerQueue<std::pair<uint64_t, uint64_t>> TimestampQueue;
  std::map<uint64_t, std::unique_ptr<TimestampQueue>> m_timestamp_map;
  // Queues for the collection channels only: to be used in the trigger primitive finding
  std::map<uint64_t, std::unique_ptr<TriggerPrimitiveFinder>> m_tp_finders;

  // Threads
  std::vector<std::thread> m_netioSubscribers;
  //std::vector<std::unique_ptr<NetioSubscriber>> m_subscribers;
  std::vector<std::unique_ptr<ReusableThread>>  m_extractors;
  std::vector<std::function<void()>> m_functors; 

  // Reordering and compression utilities
  bool m_forceNoAVXReorder;
  std::unique_ptr<ReorderFacility> m_reorderFacility;
  std::unique_ptr<QzCompressor> m_compressionFacility;

  // Trigger bookeeping
  bool m_turnaround;
  uint_fast64_t m_positionDepth; 
  char* m_lastPosition; 
  uint_fast64_t m_lastTimestamp;

  // ArtDAQ specific
  uint64_t m_triggerTimestamp;
  uint64_t m_triggerSequenceId;
  artdaq::Fragment* m_fragmentPtr;
  artdaq::Fragment* m_fragmentPtrHits;

  // Thread control
  std::atomic<bool> m_stop_trigger;
  std::atomic<bool> m_stop_subs;
  std::atomic<bool> m_cpu_lock; 

  //char** m_bufferPtr;
  //size_t** m_bytesReadPtr;

  std::mutex m_mutex;
  std::mutex m_mtx_cleaning ;
  
};

#endif

