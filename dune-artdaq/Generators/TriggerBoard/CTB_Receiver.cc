
#include "CTB_Receiver.hh"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-raw-data/Overlays/CTB_content.h"

#include <ctime>

#include <boost/asio/read.hpp>



CTB_Receiver::CTB_Receiver( const unsigned int port, 
			    const unsigned int timeout, 
			    const unsigned int buffer_size ) : 

  _word_buffer( buffer_size ), _port ( port ), _timeout(timeout), _n_TS_words( 0 ), _has_calibration_stream( false )  {

}

CTB_Receiver::~CTB_Receiver() {

  stop() ;

}



bool CTB_Receiver::stop() {

  _stop_requested = true ; 

  if ( _word_fut.valid() ) {
    _word_fut.get() ;
  }

  return true ;

}


bool CTB_Receiver::start() {

  _stop_requested = false ; 

  _word_buffer.reset() ;

  _n_TS_words = 0 ;

  _word_fut = std::async( std::launch::async, & CTB_Receiver::_word_receiver, this ) ;

  return true ;
}


int CTB_Receiver::_word_receiver() {

  std::size_t n_bytes = 0 ;
  std::size_t n_words = 0 ;
  
  const size_t header_size = sizeof( ptb::content::tcp_header_t ) ;
  const size_t word_size = ptb::content::word::word_t::size_bytes ;

  dune::DAQLogger::LogDebug("CTB_Receiver") << "Header size: " << header_size << std::endl
					    << "Word size: " << word_size << std::endl ;
  
  //connect to socket
  boost::asio::io_service io_service;
  boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), _port ) );
  boost::asio::ip::tcp::socket socket(io_service);
  
  dune::DAQLogger::LogInfo("CTB_Receiver") << "Watiting for an incoming connection on port " << _port << std::endl;

  std::future<void> accepting = async( std::launch::async, [&]{ acceptor.accept(socket) ; } ) ;

  while ( ! _stop_requested.load() ) {
    if ( accepting.wait_for( _timeout ) == std::future_status::ready ) 
      break ;
  }
  
  dune::DAQLogger::LogInfo("CTB_Receiver") << "Connection received: start reading" << std::endl;

  ptb::content::tcp_header_t head ;
  ptb::content::word::word temp_word ;

  boost::system::error_code receiving_error;
  bool connection_closed = false ;

  while ( ! _stop_requested.load() ) {
    
    _update_calibration_file() ;

    if ( ! _read( head, socket ) ) { 
      connection_closed = true ;
      break;
    }
    
    n_bytes = head.packet_size ;
    //    dune::DAQLogger::LogInfo("CTB_Receiver") << "Package size "  << n_bytes << std::endl ;
    
    // extract n_words
    n_words = n_bytes / word_size ; 

    // read n words as requested from the header
    for ( unsigned int i = 0 ; i < n_words ; ++i ) {

      //read a word
      if ( ! _read( temp_word, socket ) ) {
	connection_closed = true ;
	break ;
      }
         
      // put it in the caliration stream
      if ( _has_calibration_stream ) { 
	
	_calibration_file.write( reinterpret_cast<const char*>( temp_word.get_bytes() ), word_size ) ;
	//dune::DAQLogger::LogInfo("CTB_Receiver") << "Word Type: " << temp_word.frame.word_type << std::endl ;

      }  // word printing in calibration stream
      
      //check if it is a TS word and increment the counter
      if ( CTB_Receiver::IsTSWord( temp_word ) ) {
	_n_TS_words++ ;
      }
      else if ( CTB_Receiver::IsFeedbackWord( temp_word ) ) {

	ptb::content::word::feedback_t * feedback = reinterpret_cast<ptb::content::word::feedback_t*>( & temp_word.frame ) ;
	dune::DAQLogger::LogError("CTB_Receiver") << "Feedback word: " << std::endl 
						    << " \t TS -> " << feedback -> timestamp << std::endl 
						    << " \t Code -> " << feedback -> code << std::endl 
						    << " \t Source -> " << feedback -> source << std::endl 	  
						    << " \t Padding -> " << feedback -> padding << std::endl ;     
      }
      
      
      // push the word
      while ( ! _word_buffer.push( temp_word ) && ! _stop_requested.load() ) {
	dune::DAQLogger::LogWarning("CTB_Receiver") << "Word Buffer full and cannot store more" << std::endl ;	
      }
      
      if ( _stop_requested.load() ) break ;
      
    } // n_words loop

    if ( connection_closed ) 
      break ;

  }  // stop not requested

  socket.close() ;
  dune::DAQLogger::LogInfo("CTB_Receiver") << "Connection closed: stop receiving data from the CTB" << std::endl ;
  // return because _stop_requested
  return 0 ;
}


bool CTB_Receiver::SetCalibrationStream( const std::string & string_dir, 
					 const std::chrono::minutes & interval ) {

  _calibration_dir = string_dir ;
  if ( _calibration_dir.back() != '/' ) _calibration_dir += '/' ;

  _calibration_file_interval = interval ;

  // possibly we could check here if the directory is valid and  writable before assuming the calibration stream is valid
  return _has_calibration_stream = true ;

}


bool CTB_Receiver::IsTSWord( const ptb::content::word::word & w ) noexcept { 

  //dune::DAQLogger::LogInfo("CTB_Receiver") << "word type " <<  w.frame.word_type  << std::endl ;

  if ( w.frame.word_type == ptb::content::word::t_ts ) { 

    return true ;
    
  }
  
  return false ;
  
}


bool CTB_Receiver::IsFeedbackWord( const ptb::content::word::word & w ) noexcept {
  
  if ( w.frame.word_type == ptb::content::word::t_fback ) {

    return true ;

  }

  return false ;

}




void CTB_Receiver::_init_calibration_file() {

  if ( ! _has_calibration_stream ) return ; 
  
  char file_name[200] = "" ;
  
  time_t rawtime;
  time( & rawtime ) ;

  struct tm * timeinfo = localtime( & rawtime ) ;

  strftime( file_name, sizeof(file_name), "%F.%T.calib.txt", timeinfo );
  
  std::string global_name = _calibration_dir + file_name ;

  _calibration_file.open( global_name, std::ofstream::binary ) ;

  _last_calibration_file_update = std::chrono::steady_clock::now();

  // _calibration_file.setf ( std::ios::hex, std::ios::basefield );  
  // _calibration_file.unsetf ( std::ios::showbase );          

  dune::DAQLogger::LogDebug("CTB_Receiver") << "New Calibration Stream file: " << global_name << std::endl ;

}


void CTB_Receiver::_update_calibration_file() {

  if ( ! _has_calibration_stream ) return ; 

  std::chrono::steady_clock::time_point check_point = std::chrono::steady_clock::now();
  
  if ( check_point - _last_calibration_file_update < _calibration_file_interval ) return ; 

  _calibration_file.close() ;
  
  _init_calibration_file() ;
  
}


template<class T>
bool CTB_Receiver::_read( T & obj , boost::asio::ip::tcp::socket & socket ) {

  boost::system::error_code receiving_error;

  // std::future<std::size_t> reading = async( std::launch::async, 
  // 					    [&]{ return boost::asio::read( socket, 
  // 									   boost::asio::buffer( obj, sizeof(T) ),  
  // 									   receiving_error ) ; } ) ;
  
  // while( ! _stop_requested.load() ) {
    
  //   if ( reading.wait_for( _timeout ) != std::future_status::ready ) {
  //     continue ;
  //   }
    
  // }

  boost::asio::read( socket, boost::asio::buffer( & obj, sizeof(T) ), receiving_error ) ;

  if ( receiving_error == boost::asio::error::eof)
    return false ;
  else if ( receiving_error )
    return false ;

  return true ;
  
}
  

