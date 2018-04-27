#ifndef DUNE_ARTDAQ_CTB_RECEIVER_HH
#define DUNE_ARTDAQ_CTB_RECEIVER_HH

#include <string>
#include <atomic>
#include <chrono>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include "fhiclcpp/fwd.h"
#include "dune_artdaq/dune-artdaq/Generators/TriggerBoard/content.h"

class CTB_Receiver {

  CTB_Receiver( unsigned int port, std::chrono::milliseconds timeout = 500 ) ;
  virtual ~CTB_Receiver() ;

  bool pop( ?? & u ) ;
  bool pop( vector<??> & v ) ;
  
  bool stop() ; 

private:

  boost::lockfree::spsc_queue< ??? , boost::lockfree::capacity<1024> > _buffer ;  

  unsigned int port ;

  std::chrono::milliseconds timeout = 500 ;
  
  atomic<bool> _stop_requested = false ;
  atomic<bool> _timed_out = false ;


  
};


#endif
