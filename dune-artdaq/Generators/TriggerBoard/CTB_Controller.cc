#include "CTB_Controller.hh"

#include <sstream>

#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "json/json.h"
#include "json/reader.h"


CTB_Controller::CTB_Controller( const std::string & host, const uint16_t & port ) :
  _ios(),
  _endpoint(),
  _socket( _ios ) ,
  stop_req_( false ),
  is_running_ (false), 
  is_conf_(false)
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

bool CTB_Controller::send_reset() {

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a reset" << std::endl;

  bool ret = send_message( "{\"command\":\"HardReset\"}" ) ;

  // decide what to do with errors

  stop_req_.store( false ) ;
  is_running_.store( false ) ;
  is_conf_ = false ;

  return ret ;
}

bool CTB_Controller::send_start() {

  if (! is_conf_) {
    dune::DAQLogger::LogError("CTB_Controller") << "ERROR: Can't start a run without configuring first" << std::endl;
    return false ;
  }

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a start run" << std::endl;

  if ( send_message( "{\"command\":\"StartRun\"}" )  ) {
    
    is_running_.store(true);
    return true ;
  }

  return false ;

}

bool CTB_Controller::send_stop() {

  if ( ! is_running_.load() ) {
    dune::DAQLogger::LogWarning("CTB_Controller") << "Stop requested while not runninng" ;
    return true ;
  }

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending a stop run" << std::endl;

  bool ret = send_message( "{\"command\":\"StopRun\"}" ) ;

  is_running_.store( false ) ;
  stop_req_.store( true ) ;

  return ret ;
}



bool CTB_Controller::send_config( const std::string & config ) {
  
  if ( is_conf_ ) {
    dune::DAQLogger::LogInfo("CTB_Controller") << "Resetting before configuring" << std::endl;
    send_reset();
  }

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending config" << std::endl;

  if ( send_message( config ) ) {

    is_conf_ = true ;
    return true ;
  }

  return false ;
  
}



bool CTB_Controller::send_message( const std::string & msg ) {

  //add error options                                                                                                
  boost::system::error_code error;

  dune::DAQLogger::LogInfo("CTB_Controller") << "Sending message: " << msg << std::endl;
  
  boost::asio::write( _socket, boost::asio::buffer( msg ), error ) ;

  boost::array<char, 1024> reply_buf{" "} ;

  _socket.read_some( boost::asio::buffer(reply_buf ), error);

  std::stringstream raw_answer( std::string(reply_buf .begin(), reply_buf .end() ) ) ;
  
  dune::DAQLogger::LogDebug("CTB_Controller") << "Unformatted answer: " << std::endl << raw_answer.str() << std::endl; 

  Json::Value answer ;
  raw_answer >> answer ;

  Json::Value & messages = answer["feedback"] ;

  dune::DAQLogger::LogInfo("CTB_Controller") << "Received messages: " << messages.size() << std::endl;

  bool ret = true ;

  for ( Json::Value::ArrayIndex i = 0; i != messages.size(); ++i ) {

    std::string type = messages[i]["type"].asString() ;
    
    if ( type.find("error") != std::string::npos || type.find("Error") != std::string::npos || type.find("ERROR") != std::string::npos ) {
      dune::DAQLogger::LogError("CTB_Controller") << "Error from the Board: " << messages[i]["message"].asString() << std::endl ;
      ret = false ;
    }
    else if ( type.find("warning") != std::string::npos || type.find("Warning") != std::string::npos || type.find("WARNING") != std::string::npos ) {
      dune::DAQLogger::LogWarning("CTB_Controller") << "Warning from the Board: " << messages[i]["message"].asString() << std::endl;
    }
    else if ( type.find("info") != std::string::npos || type.find("Info") != std::string::npos || type.find("INFO") != std::string::npos) {
      dune::DAQLogger::LogInfo("CTB_Controller") << "Info from the board: " << messages[i]["message"].asString() << std::endl;
    }
    else {
      std::stringstream blob;
      blob << messages[i] ;
      dune::DAQLogger::LogInfo("CTB_Controller") << "Unformatted from the board: " << blob.str() << std::endl;
    }
  }
  
 
  return ret ;
}
