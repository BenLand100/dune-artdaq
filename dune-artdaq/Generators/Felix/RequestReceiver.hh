/*
 * L1Receiver.h
 *
 *  Created on: Feb 15, 2016
 *      Author: Enrico.Gamberini@cern.ch
 */

#ifndef SRC_REQUESTRECEIVER_H_
#define SRC_REQUESTRECEIVER_H_

#include <thread>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include "Utilities.h"
#include "ProducerConsumerQueue.h"
#include "artdaq/DAQrate/detail/RequestMessage.hh"

class RequestReceiver {
public:
  RequestReceiver(); 
  ~RequestReceiver();

  // Custom types
  typedef folly::ProducerConsumerQueue<artdaq::detail::RequestPacket> RequestQueue_t;
  typedef std::unique_ptr<RequestQueue_t> RequestQueuePtr_t;

  // Main worker function
  void thread();

  // Functionalities
  bool setup(const std::string& listenAddress,
             const std::string& multicastAddress,
             const unsigned short& multicastPort,
             const unsigned short& requestSize);
  void start();
  void stop();
  bool requestAvailable();
  bool getNextRequest(artdaq::detail::RequestPacket& request);

private:
  // Socket
  boost::asio::io_service m_io_service;
  boost::asio::ip::udp::socket m_socket;
  boost::asio::ip::udp::endpoint m_remote_endpoint;

  // Configuration
  std::string m_listenAddress;
  std::string m_multicastAddress;
  unsigned short m_multicastPort;
  unsigned short m_max_requests;

  // Request queue and thread
  RequestQueuePtr_t m_req;
  std::thread m_receiver;
  std::atomic<bool> m_stop_thread;

};

#endif /* SRC_REQUESTRECEIVER_H_ */

