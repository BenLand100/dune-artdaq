#ifndef QUEUE_HANDLER_HH_
#define QUEUE_HANDLER_HH_

#include "ProducerConsumerQueue.hh"
//#include "NetioSubscriber.hh"
#include "NetioWIBRecords.hh"
#include "Utilities.hh"
#include "Types.hh"

#include "ReusableThread.hh"

//#include "ReorderFacility.hh"
//#include "QzCompressor.hh"

//#include "netio/netio.hpp"

#include <map>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <algorithm>


/*
 * QueueHandler
 * Description: Queues for onHost mode, with NUMA aware allocation tricks
 * Author: Roland.Sipos@cern.ch
*/
class QueueHandler
{

  // Task for triggerMatchers (TBA, not used)
  class TriggerTask {
  private:
    unsigned m_id;
  public:
    TriggerTask(unsigned id) : m_id{id} { }
    ~TriggerTask(){ }
    void work() { }
  };
  template <typename T> struct triggerInvoker { void operator()(T& it) const {it->work();} };

public:
  // Singleton
  static QueueHandler& getInstance(){
    static QueueHandler myInstance;
    return myInstance;
  }

  // Prevent copying and moving.
  QueueHandler(QueueHandler const&) = delete;             // Copy construct
  QueueHandler(QueueHandler&&) = delete;                  // Move construct
  QueueHandler& operator=(QueueHandler const&) = delete;  // Copy assign
  QueueHandler& operator=(QueueHandler &&) = delete;      // Move assign 

  // Enable a channel/elink (prepare queue-socket pairs and map it.)
  bool addQueue(uint64_t chn, size_t queueSize);
//  bool addBlockQueue(uint64_t chn, size_t queueSize);
  void allocateQueues();
  UniqueFrameQueue& getQueue(uint64_t chn) { return std::ref(m_pcqs[chn]); }
//  UniqueBlockQueue& getBlockQueue(uint64_t chn) { return std::ref(m_bpcqs[chn]); }

  // Queue utils if needed
  bool flushQueues();
  size_t getNumOfChannels() { return m_activeChannels; } // Get the number of active channels.

protected:
  // Singleton
  QueueHandler(std::string contextStr, std::string felixHost, 
               uint16_t felixTXPort, uint16_t felixRXPort,
               size_t queueSize, bool verbose); 
  QueueHandler();
  ~QueueHandler();

private:
  // Constants
  const uint32_t m_headersize = sizeof(FromFELIXHeader);
  const uint_fast64_t m_tickdist=25; 

  // Configuration
  size_t m_activeChannels;
  size_t m_queueSize;
  std::vector<uint64_t> m_channels;
  std::atomic<unsigned long> m_nmessages;
  size_t m_framesize;
  size_t m_msgsize;
  size_t m_timeWindow;
  size_t m_windowOffset;
  bool m_doReorder;
  bool m_doCompress;
  bool m_qatReady;
  bool m_extract;
  bool m_verbose;

  //Precomputed bytesizes - Thijs
  size_t m_timeWindowByteSizeIn;
  size_t m_timeWindowByteSizeOut;
  size_t m_timeWindowNumFrames;
  size_t m_timeWindowNumMessages;

  // Queues 
  std::map<uint64_t, UniqueFrameQueue> m_pcqs;
  std::map<uint64_t, UniqueBlockQueue> m_bpcqs;

  // Alloc function for NUMA tricking...
  std::function<void()> m_alloc_func;
  std::unique_ptr<ReusableThread> m_alloc_thread;
  
};

#endif

