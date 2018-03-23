#ifndef NETIO_HANDLER_HH_
#define NETIO_HANDLER_HH_

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "ProducerConsumerQueue.h"
#include "ReusableThread.hh"
#include "NetioWIBRecords.h"
#include "Utilities.h"

#include "netio/netio.hpp"

#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>

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
    // Return a reference to our instance.
    return myInstance;

  }

  // Prevent copying and moving.
  NetioHandler(NetioHandler const&) = delete;             // Copy construct
  NetioHandler(NetioHandler&&) = delete;                  // Move construct
  NetioHandler& operator=(NetioHandler const&) = delete;  // Copy assign
  NetioHandler& operator=(NetioHandler &&) = delete;      // Move assign 

  // Custom types
  typedef folly::ProducerConsumerQueue<WIB_CHAR_STRUCT> FrameQueue;
  typedef std::unique_ptr<FrameQueue> UniqueFrameQueue;

  void setTimeWindow(size_t timeWindow) { m_timeWindow = timeWindow; }
  void setMessageSize(size_t messageSize) { m_msgsize = messageSize; }
  void setExtract(bool extract) { m_extract = extract; }
  void setVerbosity(bool v){ m_verbose = v; }

  // Functionalities
  // Setup context:
  bool setupContext(std::string contextStr);
  bool stopContext();
  // Enable an elink (prepare a queue, socket-pairs and sub to elink.
  bool addChannel(uint64_t chn, uint16_t tag, std::string host, uint16_t port, size_t queueSize);   
  bool busy(); // are trigger matchers busy
  void startTriggerMatchers(); // Starts trigger matcher threads.
  void stopTriggerMatchers();  // Stops trigger matcher threads.
  void startSubscribers(); // Starts the subscriber threads.
  void stopSubscribers();  // Stops the subscriber threads.
  void lockSubsToCPUs(); // Lock subscriber threads to CPUset.
  
  // ArtDAQ specific
  void setReadoutBuffer(char* buffPtr, size_t* bytePtr) { m_bufferPtr=&buffPtr; m_bytesReadPtr=&bytePtr; };
  bool triggerWorkers(uint64_t timestamp, uint64_t sequence_id, std::unique_ptr<artdaq::Fragment>& frag);

  // Queue utils
  //RS -> It's rly not a good idea to expose queues... Removed getQueues.
  //std::vector<uint32_t> pushOut(uint64_t chn); // Push out every records from channel queue.
  //SharedQueue& getQueue(uint64_t chn){ return m_pcqs[chn]; } // Access for elink's queue.  
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
  netio::context * m_context; // context
  std::thread m_netio_bg_thread; 
  std::map<uint64_t, netio::subscribe_socket*> m_sub_sockets; // subscribe sockets.
  size_t m_activeChannels;

  //Configuration:
  std::vector<uint64_t> m_channels;
  std::atomic<unsigned long> m_nmessages;
  size_t m_msgsize;
  size_t m_timeWindow;
  bool m_extract;
  bool m_verbose;

  // Queues
  std::map<uint64_t, UniqueFrameQueue> m_pcqs; // Queues for elink RX.

  // Threads
  std::vector<std::thread> m_netioSubscribers;
  std::vector<std::unique_ptr<ReusableThread>> m_extractors;
  std::vector<std::function<void()>> m_functors; 

  // ArtDAQ specific
  uint64_t m_triggerTimestamp;
  uint64_t m_triggerSequenceId;
  artdaq::Fragment* m_fragmentPtr;

  // Thread control
  std::atomic<bool> m_stop_trigger;
  std::atomic<bool> m_stop_subs;
  std::atomic<bool> m_cpu_lock; 


  char** m_bufferPtr;
  size_t** m_bytesReadPtr;

  std::mutex m_mutex;
};

#endif

