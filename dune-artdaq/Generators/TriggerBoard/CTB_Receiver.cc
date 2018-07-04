
#include "CTB_Receiver.hh"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <ctime>


CTB_Receiver::CTB_Receiver( const unsigned int port, const unsigned int timeout ) : 

  _port ( port ), _timeout( std::chrono::microseconds(timeout) ), _n_TS_words( 0 ), _has_calibration_stream( false )  {

  _stop_requested = false ;

  _raw_fut  = std::async( std::launch::async, & CTB_Receiver::_raw_receiver,  this ) ;

  _word_fut = std::async( std::launch::async, & CTB_Receiver::_word_receiver, this ) ;

}

CTB_Receiver::~CTB_Receiver() {

  stop() ;

}



bool CTB_Receiver::stop() {

  _stop_requested = true ; 

  _raw_fut.get() ;

  _word_fut.get() ;

  return true ;
}


int CTB_Receiver::_raw_receiver() {

  boost::asio::io_service io_service;

  boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v4(), _port ) );

  boost::asio::ip::tcp::socket socket(io_service);

  dune::DAQLogger::LogInfo("CTB_Receiver") << "Watiting for an incoming connection on port " << _port << std::endl;

  //std::future<void> accepting = async( std::launch::async, & boost::asio::ip::tcp::acceptor::accept, & acceptor, socket ) ;
  std::future<void> accepting = async( std::launch::async, [&]{ acceptor.accept(socket) ; } ) ;

  while ( ! _stop_requested ) {
    
    if ( accepting.wait_for( _timeout.load() ) == std::future_status::ready ) 
      break ;
    // should this accept be async to avoid long block?
      
  }
  
  dune::DAQLogger::LogInfo("CTB_Receiver") << "Connection received: start reading" << std::endl;
  
  constexpr unsigned int max_size = 4096 ;
  uint8_t tcp_data[max_size];

  std::future<std::size_t> reading ; 
  bool timed_out = false ;

  boost::system::error_code receiving_error;
  
  while ( ! _stop_requested ) {

    if ( ! timed_out )  // no workng reading thread, need to create one
      reading = async( std::launch::async, [&]{ return socket.read_some( boost::asio::buffer(tcp_data, max_size), receiving_error ) ; } ) ;
    
    if ( reading.wait_for( _timeout.load() ) != std::future_status::ready ) {
      timed_out = true ;
      continue ;
    }
    
    timed_out = false ; 

    if ( receiving_error == boost::asio::error::eof)
      break; // Connection closed cleanly by the board

    const std::size_t read_bytes = reading.get() ;
    
    if ( read_bytes > 0 ) {
      
      _raw_buffer.push( tcp_data, read_bytes ) ;
      
    }

  }

  // return because _stop_requested
  return 0 ;
}


int CTB_Receiver::_word_receiver() {

  std::size_t n_bytes = 0 ;
  std::size_t n_words = 0 ;
  
  const size_t header_size = sizeof( ptb::content::tcp_header_t ) ;
  const size_t word_size = ptb::content::word::word_t::size_bytes ;

  dune::DAQLogger::LogDebug("CTB_Receiver") << "Header size: " << header_size << std::endl
					   << "Word size: " << word_size << std::endl ;
  
  while ( ! _stop_requested ) {
    
    _update_calibration_file() ;

    // look for an header 
    while ( _raw_buffer.read_available() < header_size && ! _stop_requested ) {
      ; //do nothing
    }

    ptb::content::tcp_header_t head ;
    
    _raw_buffer.pop( reinterpret_cast<uint8_t*>( & head ), header_size );
    
    n_bytes = head.packet_size ;
    //    dune::DAQLogger::LogInfo("CTB_Receiver") << "Package size "  << n_bytes << std::endl ;
    
    // extract n_words
    n_words = n_bytes / word_size ; 

    ptb::content::word::word temp_word ;

    // read n words as requested from the header
    for ( unsigned int i = 0 ; i < n_words ; ++i ) {
         
      while ( _raw_buffer.read_available() < word_size && ! _stop_requested ) {
	; //do nothing
      }

      //read a word
      _raw_buffer.pop( temp_word.get_bytes(), word_size ) ;

      // put it in the caliration stream
      if ( _has_calibration_stream ) { 
	
	_calibration_file.write( reinterpret_cast<const char*>( temp_word.get_bytes() ), word_size ) ;
	//dune::DAQLogger::LogInfo("CTB_Receiver") << "Word Type: " << temp_word.frame.word_type << std::endl ;

      }  // word printing in calibration stream

      //check if it is a TS word and increment the counter
      if ( CTB_Receiver::IsTSWord( temp_word ) ) {
	_n_TS_words++ ;
      }


      // push the word
      _word_buffer.push( temp_word ) ;
      
      if ( _stop_requested ) break ;
      
    }

  }  // stop not requested


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

  if ( w.frame.word_type == 0x7 ) { 

    dune::DAQLogger::LogDebug("CTB_Receiver") << "TS word identified" << std::endl ;
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




