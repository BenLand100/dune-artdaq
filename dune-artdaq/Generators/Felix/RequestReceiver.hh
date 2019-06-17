/*
 * L1Receiver.h
 *
 *  Created on: Feb 15, 2016
 *      Author: Enrico.Gamberini@cern.ch
 *  Modified on: Jun 6, 2018 (GLM: get triggers from timing BR)
 */

#ifndef SRC_REQUESTRECEIVER_H_
#define SRC_REQUESTRECEIVER_H_

#include <thread>
#include <vector>
#include "zmq.h"

#include "Utilities.hh"
#include "ProducerConsumerQueue.hh"

struct TriggerInfo {
  uint64_t seqID;
  uint64_t timestamp;
};

class RequestReceiver {
public:
  RequestReceiver(std::string & addr); 
  ~RequestReceiver();

  // Custom types
  typedef folly::ProducerConsumerQueue<TriggerInfo> RequestQueue_t;
  typedef std::unique_ptr<RequestQueue_t> RequestQueuePtr_t;


  // Functionalities
  void start();
  void stop();
  TriggerInfo getNextRequest(const long timeout_ms=2000);

private:
  // Main worker function
  void thread();
  bool rcvMore();
  std::vector<uint64_t> getVals();
  void* m_socket;
  void* m_ctx;
  // Configuration
  std::string m_subscribeAddress;

  TriggerInfo m_prevTrigger;

  // Request queue and thread
  RequestQueuePtr_t m_req;
  std::thread m_receiver;
  std::atomic<bool> m_stop_thread;

};

#endif /* SRC_REQUESTRECEIVER_H_ */

