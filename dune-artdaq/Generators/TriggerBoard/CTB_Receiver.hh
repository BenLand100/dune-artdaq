#ifndef DUNE_ARTDAQ_CTB_RECEIVER_HH
#define DUNE_ARTDAQ_CTB_RECEIVER_HH

#include <string>
#include <atomic>
#include <chrono>
#include <future>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include "fhiclcpp/fwd.h"
#include "dune_artdaq/dune-artdaq/Generators/TriggerBoard/content.h"

/*

  The CTB receiver is a a buffer in the form of a SPSC queue.
  The _receiver method create the server and whenever data are received they are pushed in the buffer

  The calling thread of the CTB_Receiver is supposed to pop these elements out
  using pop


*/ 


class CTB_Receiver {

  CTB_Receiver( const unsigned int port, const std::chrono::milliseconds & timeout = 500 ) ;
  virtual ~CTB_Receiver() ;

  // access methods to the buffer
  // They should return words from the CTB receiver
  bool pop( ?? & u ) ;
  bool pop( vector<??> & v ) ;
  
  bool stop() ; 

private:

  boost::lockfree::spsc_queue< uint8_t , boost::lockfree::capacity<4096> > _buffer ;  

  // this is the receiver thread to be called
  // return value to be used to 
  int _receiver() ;   
  std::future<int> _fut ;


  const unsigned int _port ;

  std::chrono::milliseconds _timeout = 500 ;
  
  atomic<bool> _stop_requested = false ;
  atomic<bool> _timed_out = false ;


  
};


#endif
