#include "CTB_Receiver.hh"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"


CTB_Receiver::CTB_Receiver( const unsigned int port, const unsigned int timeout ) : 

  _port ( port ), _timeout( std::chrono::milliseconds(timeout) ) {

  _stop_requested = false ;

  _fut = std::async( std::launch::async, & CTB_Receiver::_receiver, this ) ;

}

CTB_Receiver::~CTB_Receiver() {

  stop() ;

}

// access methods to the buffer
//bool pop( ?? & u ) ;
//bool pop( vector<??> & v ) ;


bool CTB_Receiver::stop() {

  _stop_requested = true ; 

  _fut.get() ;

  return true ;
}


int CTB_Receiver::_receiver() {

  boost::asio::io_service io_service;

  boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint( boost::asio::ip::tcp::v6(), _port ) );

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
  
  constexpr unsigned int max_size = 1024 ;
  uint8_t tcp_data[max_size];

  std::future<std::size_t> reading ; 
  bool timed_out = false ;
  
  while ( ! _stop_requested ) {

    if ( ! timed_out )  // no workng reading thread, need to create one
      reading = async( std::launch::async, [&]{ return socket.read_some( boost::asio::buffer(tcp_data, max_size) ) ; } ) ;
    
    if ( reading.wait_for( _timeout.load() ) != std::future_status::ready ) {
      timed_out = true ;
      continue ;
    }
    
    timed_out = false ;
    const std::size_t read_bytes = reading.get() ;
    
    if ( read_bytes > 0 ) 
      _buffer.push( tcp_data, read_bytes ) ;

  }

  // return because _stop_requested
  return 0 ;
}


