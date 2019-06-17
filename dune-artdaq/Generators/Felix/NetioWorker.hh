#ifndef NETIO_WORKER_HH_
#define NETIO_WORKER_HH_

#include "ProducerConsumerQueue.h"

#include <netio/netio.hpp>

#include <thread>
#include <iostream>
#include <vector>
#include <bitset>
#include <string>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

class NetioWorker {

public:

  /* Explicitly using the default constructor to
  *  underline the fact that it does get called. */
  NetioWorker(netio::subscribe_socket& subscribe_socket,
                  folly::ProducerConsumerQueue<netio::message>& producer_consumer_queue)
    : worker_thread(), sub_socket{ subscribe_socket }, pcq{ producer_consumer_queue }
  { 
    std::cout << "WOOF -> NetioWorker:: constructor called!!!\n";
  }

  ~NetioWorker() {
    std::cout << "WOOF -> NetioWorker:: destructor called!!!\n";
    std::cout << "     -> RIP " << whatAmI << "\n";
    stop_thread = true;
    if (worker_thread.joinable())
      worker_thread.join();
  }

  // This may be a bad idea! Should not move at all?
  // Or this is good, just passing the refs?
  // Otherwise force delete: NetioWorker(NetioWorker&& other) = delete;
  NetioWorker(NetioWorker&& other) //noexcept {
    : sub_socket( std::ref(other.sub_socket) ),
      pcq( std::ref(other.pcq) ) {
  }

  // Actually start the thread. Notice Move semantics!
  void queueIn() {
    whatAmI = "subscriber";
    std::cout << "WOOF -> NetioWorker:: moved to become a subscriber...\n";
    worker_thread = std::thread(&NetioWorker::QueueMessages, this);
    //worker_thread.join();
  }

  void queueOut( char* buffer, size_t* bytes_read ) {
    whatAmI="dequeuer";
//    std::cout << "WOOF -> NetioWorker:: moved to become a dequeuer...\n";
    worker_thread = std::thread(&NetioWorker::DequeueMessages, this, buffer, bytes_read);
    worker_thread.join();
  }

  void join() {
    std::cout<<"BLA Called join"<<std::endl;
    stop_thread = true;
    std::cout<<"BLA stop_thread true"<<std::endl;
    if (worker_thread.joinable()) {
      std::cout<<"BLA is joinable"<<std::endl;
      worker_thread.join();
    }
    std::cout<<"BLA after if and join"<<std::endl;
  }

private:
  // Actual thread handler members.
  std::string whatAmI = "sad-pepe";
  std::thread worker_thread;
  std::atomic_bool stop_thread{ false };

  // References coming from the HWInterface.
  netio::subscribe_socket& sub_socket;
  folly::ProducerConsumerQueue<netio::message>& pcq;


  // The main working thread.
  void QueueMessages() {
    
    while(!stop_thread) {
//      std::cout << "WOOF -> I won't stop, as stop_thread is: " << stop_thread << std::endl;
      // subscribe to netio socket and read messages constantly.
      netio::message m;
//      std::cout << "WOOF -> Message object created... Before receive...\n";
      sub_socket.recv(m);
//      std::cout << "WOOF -> Received message...\n";
      pcq.write( std::move(m) );
//      std::cout << "WOOF -> Queue populated! \n";
    }
  }

  void DequeueMessages( char* buffer, size_t* bytes_read ) {
      netio::message msgObj;

      while(!pcq.read( std::ref(msgObj)))
      {}
      *bytes_read = msgObj.size()-8; //eg171012 remove felix header
      memcpy(buffer, msgObj.data_copy().data()+8,msgObj.size()-8);

  }

};

#endif /* NETIO_WORKER_HH_ */
