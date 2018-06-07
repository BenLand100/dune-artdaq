#ifndef NETIO_SUBSCRIBER_HH_
#define NETIO_SUBSCRIBER_HH_

#include "ProducerConsumerQueue.hh"

#include <netio/netio.hpp>

#include <thread>
#include <iostream>
#include <vector>
#include <bitset>
#include <string>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

class NetioSubscriber {

public:

  /* Explicitly using the default constructor to
  *  underline the fact that it does get called. */
  NetioSubscriber(netio::subscribe_socket& subscribe_socket,
                  folly::ProducerConsumerQueue<netio::message>& producer_consumer_queue)
    : m_worker_thread(), m_sub_socket{ subscribe_socket }, m_pcq{ producer_consumer_queue }
  { 
    std::cout << "WOOF -> NetioSubscriber:: constructor called!!!\n";
  }

  ~NetioSubscriber() {
    std::cout << "WOOF -> NetioSubscriber:: destructor called!!!\n";
    std::cout << "     -> RIP " << m_taskname << "\n";
    m_stop_thread = true;
    if (m_worker_thread.joinable())
      m_worker_thread.join();
  }

  // This may be a bad idea! Should not move at all?
  // Or this is good, just passing the refs?
  // Otherwise force delete: NetioSubscriber(NetioSubscriber&& other) = delete;
  NetioSubscriber(NetioSubscriber&& other) //noexcept {
    : m_sub_socket( std::ref(other.m_sub_socket) ),
      m_pcq( std::ref(other.m_pcq) ) {
  }

  // Actually start the thread. Notice Move semantics!
  void queueIn() {
    m_taskname = "netio-subscriber";
    std::cout << "WOOF -> NetioSubscriber:: moved to become a subscriber...\n";
    m_worker_thread = std::thread(&NetioSubscriber::QueueMessages, this);
  }

  void join() {
    std::cout<<"BLA Called join"<<std::endl;
    m_stop_thread = true;
    std::cout<<"BLA stop_thread true"<<std::endl;
    if (m_worker_thread.joinable()) {
      std::cout<<"BLA is joinable"<<std::endl;
      m_worker_thread.join();
    }
    std::cout<<"BLA after if and join"<<std::endl;
  }

private:
  // Actual thread handler members.
  std::string m_taskname = "netio-blank";
  std::thread m_worker_thread;
  std::atomic_bool m_stop_thread{ false };

  // References coming from the HWInterface.
  netio::subscribe_socket& m_sub_socket;
  folly::ProducerConsumerQueue<netio::message>& m_pcq;


  // The main working thread.
  void QueueMessages() {
    
    while(!m_stop_thread) {
//      std::cout << "WOOF -> I won't stop, as stop_thread is: " << stop_thread << std::endl;
      // subscribe to netio socket and read messages constantly.
      netio::message m;
//      std::cout << "WOOF -> Message object created... Before receive...\n";
      m_sub_socket.recv(m);
//      std::cout << "WOOF -> Received message...\n";
      m_pcq.write( std::move(m) );
//      std::cout << "WOOF -> Queue populated! \n";
    }
  }

};

#endif /* NETIO_SUBSCRIBER_HH_ */

