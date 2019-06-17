#ifndef NETIO_WORKER_NEW_HH_
#define NETIO_WORKER_NEW_HH_

#include "SmartCircularBuffer.h"

#include <netio/netio.hpp>

#include <thread>
#include <iostream>
#include <vector>
#include <bitset>
#include <string>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

class NetioWorkerNew {

public:

  /* Explicitly using the default constructor to
  *  underline the fact that it does get called. */
  NetioWorkerNew(netio::subscribe_socket& subscribe_socket,
                  SmartCircularBuffer<netio::message>& producer_consumer_queue)
    : worker_thread(), sub_socket{ subscribe_socket }, pcq{ producer_consumer_queue }
  { 
    std::cout << "WOOF -> NetioWorkerNew:: constructor called!!!\n";
    std::cout << "FUNCTIONALITY: \n";
    std::string str("WOOF WOOF");
    netio::message msg(str);
    std::cout << " MSG SIZE: " << msg.size() << '\n'; 
  }

  ~NetioWorkerNew() {
    std::cout << "WOOF -> NetioWorkerNew:: destructor called!!!\n";
    stop_thread = true;
    if (worker_thread.joinable())
      worker_thread.join();
  }

  // This may be a bad idea! Should not move at all?
  // Or this is good, just passing the refs?
  // Otherwise force delete: NetioWorkerNew(NetioWorkerNew&& other) = delete;
  NetioWorkerNew(NetioWorkerNew&& other) //noexcept {
    : sub_socket( std::ref(other.sub_socket) ),
      pcq( std::ref(other.pcq) ) {
  }

  // Actually start the thread. Notice Move semantics!
  void queueIn() {
    std::cout << "WOOF -> NetioWorkerNew:: moved to become a subscriber...\n";
    worker_thread = std::thread(&NetioWorkerNew::QueueMessages, this);
    //worker_thread.join();
  }

  void queueOut( char* buffer, size_t* bytes_read ) {
//    std::cout << "WOOF -> NetioWorkerNew:: moved to become a dequeuer...\n";
    worker_thread = std::thread(&NetioWorkerNew::DequeueMessages, this, buffer, bytes_read);
    worker_thread.join();
  }

  void join() {
    stop_thread = true;
    if (worker_thread.joinable()) {
      worker_thread.join();
    }
  }

private:
  // Actual thread handler members.
  std::thread worker_thread;
  std::atomic_bool stop_thread{ false };

  // References coming from the HWInterface.
  netio::subscribe_socket& sub_socket;
  SmartCircularBuffer<netio::message>& pcq;


  // The main working thread.
  void QueueMessages() {
    //uint64_t * timestamp;
    uint64_t overwrite=0;
    while(!stop_thread) {
      // subscribe to netio socket and read messages constantly.
      //uint64_t * timestamp;
      netio::message m;
      sub_socket.recv(m);
      //std::cout << "Num of fragments in message: " << m.num_fragments() << std::endl;
      const netio::message::fragment* firstFrg = m.fragment_list();
      //std::cout << "First fragment size: " << firstFrg->size << std::endl;

      uint64_t *timestamp = const_cast<uint64_t*>(reinterpret_cast<const uint64_t*>(firstFrg->data+16));
      
      //std::cout<<"BLA -> message size "<<m.size()<<std::endl;
 //     std::cout<<"cast from message                 0x"<<std::hex<< *timestamp <<std::dec<< std::endl;
      *timestamp = overwrite;
   //   std::cout<<"cast from message after overwrite 0x"<<std::hex<< *timestamp <<std::dec<< std::endl;
      overwrite+=25;
      pcq.write( std::move(m) );
//      pcq.write( m );
    }
  }

  void DequeueMessages( char* buffer, size_t* bytes_read ) {
    netio::message msg;

    while(!pcq.read( std::ref(msg)))
//    while(!pcq.read(msg))
    {}
//    msg = pcq.read();
    *bytes_read = msg.size()-8; //eg171012 remove felix header
    memcpy(buffer, msg.data_copy().data()+8,msg.size()-8);

//    std::cout<<"Reading in Deque"<<std::endl;
//    for(int i=2;i<8;i++){
//      std::cout<<std::hex<<((unsigned) (msg)[0+i*4] & 0xFF)<<" "<<((unsigned) (msg)[1+i*4] & 0xFF)<<" "<<((unsigned) (msg)[2+i*4] & 0xFF)<<" "<<((unsigned) (msg)[3+i*4] & 0xFF)<<" ";
//    }
//    std::cout<<std::dec<<std::endl;
     
  }

};

#endif /* NETIO_WORKER_NEW_HH_ */
