#include "CTB_Receiver.hh"

CTB_Receiver::CTB_Receiver( const unsigned int port, const std::chrono::milliseconds & timeout ) : 
  _port ( port ), _timeout( timeout ) {

  _fut = std::async( launch::async, & CTB_Receiver::_receiver, this ) ;

}

CTB_Receiver::~CTB_Receiver() {

  stop() ;

}

// access methods to the buffer
//bool pop( ?? & u ) ;
//bool pop( vector<??> & v ) ;


bool CTB_Receiver::stop() {

  _stop_requested = true ; 

  int ret = _fut.get() ;

}


int CTB_Receiver::_receiver() {

  boost::asio::io_service io_service;

  tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v6(), port ) );

  tcp::socket socket(io_service);

  std::future<void> accepting = async( launch::async, & acceptor::accept, acceptor, socket ) ;
  
  while ( ! _stop_requested ) {
    
    if ( accepting.wait_for( _timeout ) == future_status::ready ) 
      break ;
    // should this accept be async to avoid long block?
      
  }
  
  constexpr unsigned int max_size = 1024 ;
  uint8_t tcp_data[max_size];

  std::future<std::size_t> reading ; 
  bool timed_out = false ;
  
  while ( ! _stop_requested ) {

    if ( ! timed_out )  // no workng reading thread, need to create one
      reading = async( launch::async, asio::read_some, socket,  boost::asio::buffer(tcp_data, max_size)  ) ;
    
    if ( ! accepting.wait_for( _timeout ) == future_status::ready ) {
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


