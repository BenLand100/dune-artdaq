#include "CTB_Controller.hh"

#include <sstream>

#include "dune-artdaq/DAQLogger/DAQLogger.hh"

CTB_Controller::CTB_Controller( const std::string & host, const uint16_t & port ) :
  _ios(),
  _endpoint(),
  _socket( _ios ) ,
  stop_req_( false ),
  is_running_ (false), 
  is_conf_(false),
  _buf{" "}          
{
  
  boost::asio::ip::tcp::resolver resolver( _ios );
  
  boost::asio::ip::tcp::resolver::query query(host, std::to_string(port) ) ; 
  boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query) ;
 
  _endpoint = iter->endpoint() ; 
  
  //shoudl we put this into a try?
  _socket.connect( _endpoint ) ;

    
}


CTB_Controller::~CTB_Controller() { 


  //check if running. and in case stop the run
  send_stop(); 

  _socket.close() ;

}

void CTB_Controller::send_reset() {

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a reset" << std::endl;

  send_message( "{\"command\":\"HardReset\"}" ) ;

  // decide what to do with errors

  stop_req_.store( false ) ;
  is_running_.store( false ) ;
  is_conf_ = false ;

}

void CTB_Controller::send_start() {

  if (! is_conf_) {
    dune::DAQLogger::LogError("CTB_Controller") << "ERROR: Can't start a run without configuring first" << std::endl;
    return ;
  }

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a start run" << std::endl;

  send_message( "{\"command\":\"StartRun\"}" ) ;

  is_running_.store(true);

}

void CTB_Controller::send_stop() {

  if ( ! is_running_.load() ) {
    dune::DAQLogger::LogWarning("CTB_Controller") << "Stop requested while not runninng" ;
    return ;
  }

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a stop run" << std::endl;

  send_message( "{\"command\":\"StopRun\"}" ) ;

  is_running_.store( false ) ;
  stop_req_.store( true ) ;
}

void CTB_Controller::send_config( const std::string & host, const uint16_t & port, const unsigned long rollover  )  {
  
  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending config" << std::endl;
  
  if ( is_conf_ ) {
    dune::DAQLogger::LogInfo("CTB_Controller") << "Resetting before configuring" << std::endl;
    send_reset();
  }							   

  static const std::string g_config_1 = "{\"ctb\":{\"sockets\":{\"receiver\":{\"host\":\"";
  static const std::string g_config_2 = "\",\"port\":" ; 
  static const std::string g_config_3 = ",\"rollover\":" ; 
  static const std::string g_config_4 = "}},\"subsystems\":{\"ssp\":{\"dac_thresholds\":[2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018,2018]}}}}" ; 

  std::stringstream s;
  s << g_config_1 << host << g_config_2 << port << g_config_3 << rollover << g_config_4 ;
  const std::string g_config = s.str() ;
  
  send_message( g_config ) ;

  is_conf_ = true ;

}


void CTB_Controller::send_message( const std::string & msg ) {

  //add error options                                                                                                
  boost::system::error_code error;
  
  boost::asio::write( _socket, boost::asio::buffer( msg ), error ) ;

  _socket.read_some( boost::asio::buffer(_buf), error);

  dune::DAQLogger::LogInfo("CTB_Controller") << "Received answer: " << std::string(_buf.begin(), _buf.end() ) << std::endl;                                                          
  //dune::DAQLogger::LogDebug("CTB_Controller") << msg ;
    
  //decide what to do with the error
 
}
