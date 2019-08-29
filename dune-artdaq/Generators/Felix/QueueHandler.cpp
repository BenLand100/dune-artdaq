#include "QueueHandler.hh"
#include "NetioWIBRecords.hh"

#include "packetformat/block_format.hpp"

#include <ctime>
#include <iomanip>
#include <pthread.h>
#include <thread>

#define REORD_DEBUG
#define QATCOMP_DEBUG


QueueHandler::QueueHandler()
{
  m_activeChannels=0;

  m_nmessages=0;
  m_msgsize = 2784;
//  m_stop_subs=false;
//  m_cpu_lock=false;
  m_extract = true;
  //m_bufferPtr=nullptr;
  //m_bytesReadPtr=nullptr;

//  m_busyLinks.store(0);
//  m_triggerTimestamp=0;
//  m_triggerSequenceId=0;

//  m_turnaround=true;
//  m_lastPosition = nullptr;
//  m_lastTimestamp = 0x0;

  m_alloc_thread = std::make_unique<ReusableThread>(5678);
  m_alloc_func = [&] {
    for(unsigned i=0; i<m_activeChannels; ++i){
      SUPERCHUNK_CHAR_STRUCT scs; // copy into a supchunk struct
      for(unsigned int p=0; p<5568; p++){
        scs.fragments[p] = 'x';  
      }

      m_pcqs[i] = std::make_unique<FrameQueue>(m_queueSize);

      for (unsigned n=0; n<1000; ++n){
        m_pcqs[i]->write(scs);
      }

      m_pcqs[i]->popXFront(1000);
    }  
  };

  // PREPARE THREAD AFFINITY FOR ALLOCATOR
  cpu_set_t mtset;
  CPU_ZERO(&mtset);
  CPU_SET(0, &mtset);
  pthread_setaffinity_np(m_alloc_thread->get_thread().native_handle(), sizeof(cpu_set_t), &mtset);
}

QueueHandler::~QueueHandler() {
  m_pcqs.clear(); 
}

bool QueueHandler::flushQueues(){
  // while (!pcq.isEmpty()) { pcq.pop(); }
  while (!m_pcqs[0]->isEmpty()) { m_pcqs[0]->popFront(); }
  return true; ///m_pcqs[m_channels[chn]]
}

bool QueueHandler::addQueue(uint64_t chn, size_t queueSize){
  m_queueSize = queueSize;
  m_channels.push_back(chn);
  //m_pcqs[chn] = std::make_unique<FrameQueue>(queueSize);
  m_activeChannels++;
  return true;

} 

void QueueHandler::allocateQueues(){
  m_alloc_thread->set_work(m_alloc_func);
  while (!m_alloc_thread->get_readiness()) {
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
//  for(unsigned i=0; i<m_activeChannels; ++i){
//    m_pcqs[i] = std::make_unique<FrameQueue>(queueSize);
//  }  
}

