#ifndef DUNE_ARTDAQ_CTB_RECEIVER_HH
#define DUNE_ARTDAQ_CTB_RECEIVER_HH

#include <string>
#include <fstream>
#include <atomic>
#include <chrono>
#include <future>
#include <ratio>

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include "fhiclcpp/fwd.h"
#include "dune-raw-data/Overlays/CTB_content.h"

/*

  The CTB receiver is a a buffer in the form of a SPSC queue.
  The _receiver method create the server and whenever data are received they are pushed in the buffer

  The calling thread of the CTB_Receiver is supposed to pop these elements out
  using pop


*/ 


class CTB_Receiver {

public:
  CTB_Receiver( ) = delete ;
  CTB_Receiver( const unsigned int port, const unsigned int timeout = 10 /*microseconds*/ ) ;

  virtual ~CTB_Receiver() ;

  auto & Buffer() { return _word_buffer ; }

  auto & N_TS_Words() { return _n_TS_words ; }  
  
  bool SetCalibrationStream( const std::string & string_dir, 
			     const std::chrono::minutes & interval ) ; 

  bool stop() ; 

  static bool IsTSWord( const ptb::content::word::word & w ) noexcept ;
  static bool IsFeedbackWord( const ptb::content::word::word & w ) noexcept ;

private:

  // the raw buffer can contain 4 times the maximum TCP package size, which is 4 KB 
  boost::lockfree::spsc_queue< uint8_t , boost::lockfree::capacity<65536> > _raw_buffer ;  

  boost::lockfree::spsc_queue< ptb::content::word::word , boost::lockfree::capacity<4096> > _word_buffer ;  
  
  
  // this is the receiver thread to be called
  // return value to be used to 
  int _raw_receiver() ;   
  int _word_receiver() ;

  void _update_calibration_file() ;
  void _init_calibration_file() ;

  std::future<int> _raw_fut ;
  std::future<int> _word_fut ;

  const unsigned int _port ;

  std::atomic<std::chrono::microseconds> _timeout ;

  std::atomic<unsigned int> _n_TS_words ;

  std::atomic<bool> _stop_requested ;
  //atomic<bool> _timed_out = false ;

  // members related to calibration stream
  bool _has_calibration_stream ; 
  std::string _calibration_dir ; 
  std::chrono::minutes _calibration_file_interval ;  
  std::ofstream _calibration_file   ;
  std::chrono::steady_clock::time_point _last_calibration_file_update ;



};


#endif
