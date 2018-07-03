#ifndef DUNE_ARTDAQ_CTB_CONTROLLER_HH
#define DUNE_ARTDAQ_CTB_CONTROLLER_HH

#include <string>
#include <atomic>

#include <boost/asio.hpp>
#include <boost/array.hpp>

#include "fhiclcpp/fwd.h"
#include "dune_artdaq/dune-artdaq/Generators/TriggerBoard/content.h"



class CTB_Controller {

public:

  CTB_Controller( const std::string &host = "np04-ctb-1",const uint16_t &port = 8991 ) ;
  //  CTB_Controller( fhicl::ParameterSet const & ps ) ;
  virtual ~CTB_Controller() ;

  void send_reset() ;
  void send_start() ;
  void send_stop() ;
  void send_config( const std::string &host = "np04-srv-013",const uint16_t &port = 8992, unsigned long rollover = 50000 ) ;
  //void process_quit() ;

protected:

  void send_message( const std::string & msg ) ;


private:

  boost::asio::io_service _ios;
  boost::asio::ip::tcp::endpoint _endpoint;
  boost::asio::ip::tcp::socket _socket ;
  
  // control variables
  std::atomic<bool> stop_req_;
  std::atomic<bool> is_running_;
  bool is_conf_;
  //std::thread::id receiver_id_;

  // Aux vars
  boost::array<char, 1024> _buf; 

};


#endif
