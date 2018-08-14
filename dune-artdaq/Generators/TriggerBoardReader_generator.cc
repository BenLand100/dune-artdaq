#include <stdexcept>

#include "json/json.h"
#include "json/reader.h"

#include "dune-artdaq/Generators/TriggerBoardReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "artdaq/DAQdata/Globals.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "dune-raw-data/Overlays/CTB_content.h"

#include "artdaq/Application/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"

#include "canvas/Utilities/Exception.h"
#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <string>

#include <unistd.h>


dune::TriggerBoardReader::TriggerBoardReader(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  _run_controller(),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType ) ),
  throw_exception_(ps.get<bool>("throw_exception",true) )
{

  //get board address and control port from the fhicl file
  const unsigned int control_port = ps.get<uint16_t>("control_port", 8991 ) ;
  const std::string address = ps.get<std::string>( "board_address", "np04-ctb-1" );

  dune::DAQLogger::LogDebug("TriggerBoardGenerator") << "control messages sent to " << address << ":" << control_port << std::endl; 


  // get options for fragment creation 
  _group_size = ps.get<unsigned int>( "group_size", 1 ) ;
  dune::DAQLogger::LogDebug( "TriggerBoardGenerator") << "Creating fragments with " << _group_size << " packages from the CTB" ;

  _max_words_per_frag = ps.get<unsigned int>( "max_words_per_frag", 3280 ) ;

  _max_frags_per_call = ps.get<unsigned int>( "max_frags_per_call", 10 ) ;
  dune::DAQLogger::LogDebug( "TriggerBoardGenerator") << "Sending at most " << _max_frags_per_call << " fragments per getNext_ call " ;


  //build the ctb_controller 
  _run_controller.reset( new CTB_Controller( address, control_port ) ) ;

  
  // prepare the receiver 

  // get the json configuration string
  std::stringstream json_stream( ps.get<std::string>( "board_config" ) ) ;
  
  Json::Value jblob;
  json_stream >> jblob ;

  // get the receiver port from the json
  const unsigned int receiver_port = jblob["ctb"]["sockets"]["receiver"]["port"].asUInt() ; 

  const unsigned int timeout = ps.get<uint16_t>("receiver_timeout", 10 ) ; //milliseconds

  //build the ctb receiver and set its connecting port
  _receiver.reset( new CTB_Receiver( receiver_port, timeout ) ) ;
 
  // if necessary, set the calibration stream 
  if ( ps.has_key( "calibration_stream_output") ) {
    _receiver -> SetCalibrationStream( ps.get<std::string>( "calibration_stream_output"), 
				       std::chrono::minutes( ps.get<unsigned int>( "calibration_update", 5 )  ) ) ;
  }
    
  // complete the json configuration 
  // with the receiver host which is the machines where the board reader is running
  const std::string receiver_address = boost::asio::ip::host_name() ;
  
  jblob["ctb"]["sockets"]["receiver"]["host"] = receiver_address ;

  dune::DAQLogger::LogInfo("TriggerBoardGenerator") << "Board packages receved at " << receiver_address << ':' << receiver_port << std::endl; 

  // create the json string
  json_stream.str("");
  json_stream.clear() ;

  json_stream << jblob ;
  std::string config = json_stream.str() ;

  // send the configuration
  bool config_status = _run_controller -> send_config( config ) ;

 if ( ! config_status ) {
   dune::DAQLogger::LogError("TriggerBoardGenerator") << "CTB failed to configure" << std::endl ;

   if ( throw_exception_ ) {
     throw std::runtime_error("CTB failed to configure") ;
    }
  }
  
}

dune::TriggerBoardReader::~TriggerBoardReader() {

  //call destructors for ctb_controller and ctb_receiver
  _run_controller -> send_stop() ;

}

bool dune::TriggerBoardReader::getNext_(artdaq::FragmentPtrs & frags) {

  if ( should_stop() ) {
    return false;
  }

  std::size_t n_words = _receiver -> Buffer().read_available() ;
  if ( n_words == 0 ) {
    TLOG( 20, "TriggerBoardReader") << "getNext_ returns as no words are available" << std::endl ;
    return true ;
  }

  long unsigned int sent_bytes = 0 ;

  while ( ( _group_size > 0  && _receiver -> N_TS_Words().load() >= _group_size ) ||
	  ( _group_size <= 0 && n_words > 0 )                                     || 
	  ( n_words >= _max_words_per_frag ) ) {

    std::unique_ptr<artdaq::Fragment> fragptr( CreateFragment() );

    sent_bytes += fragptr -> dataSizeBytes() ;

    frags.emplace_back( std::move( fragptr ) ) ;

    if ( frags.size() >= _max_frags_per_call ) break ;

    if ( should_stop() ) break ;    

    n_words = _receiver -> Buffer().read_available() ;

  }

  TLOG( 20, "TriggerBoardReader") << "Sending " << frags.size() <<  " fragments" << std::endl ;

  if(artdaq::Globals::metricMan_ != nullptr) {
    artdaq::Globals::metricMan_->sendMetric("Fragments Sent", frags.size(), "Fragments", 0, artdaq::MetricMode::LastPoint) ;
    artdaq::Globals::metricMan_->sendMetric("Bytes Sent",     sent_bytes,   "Bytes",     0, artdaq::MetricMode::LastPoint) ;
  }
  
  return true;
}


artdaq::FragmentPtr dune::TriggerBoardReader::CreateFragment() {

  ptb::content::word::word temp_word ;

  static const constexpr std::size_t word_bytes = sizeof( ptb::content::word::word_t ) ;

  const std::size_t n_words = _receiver -> Buffer().read_available() ;

  std::size_t initial_bytes = n_words * word_bytes ;

  artdaq::Fragment::timestamp_t timestamp = artdaq::Fragment::InvalidTimestamp ;
  bool has_TS = false ;

  if ( _has_last_TS ) {
    has_TS = true ;
    timestamp = _last_timestamp ;
  }

  unsigned int word_counter = 0 ;
  unsigned int group_counter = 0 ;

  artdaq::FragmentPtr fragptr( artdaq::Fragment::FragmentBytes( initial_bytes ) ) ; 

  for ( word_counter = 0 ; word_counter < n_words ; ++word_counter ) {

    _receiver -> Buffer().pop( temp_word ) ;
    memcpy( fragptr->dataBeginBytes() + word_counter * word_bytes, temp_word.get_bytes(), 16 ) ;

    if ( CTB_Receiver::IsFeedbackWord( temp_word ) ) {
      dune::DAQLogger::LogError("TriggerBoardGenerator") << "CTB issued a feedback word" << std::endl ;
      if ( throw_exception_ ) {
	throw std::runtime_error("CTB sent a feedback word") ;
      }
    }
      
    else if ( CTB_Receiver::IsTSWord( temp_word ) ) {

      --(_receiver -> N_TS_Words()) ;
      ++group_counter ;

      if ( temp_word.frame.timestamp != 0 ) {
	_last_timestamp = temp_word.frame.timestamp ;
	_has_last_TS = true ;

	if ( ! has_TS ) {
	  has_TS = true ;
	  timestamp = _last_timestamp ;
	}
      }

      if ( _group_size > 0 ) {
	if ( group_counter >= _group_size ) {
	  TLOG( 20, "TriggerBoardReader") << "Final group_counter " << group_counter << std::endl ;
	  break ;
	}
      }

    } // if this was a TS word

  }
  
  fragptr -> resizeBytes( word_counter * word_bytes ) ;
  fragptr -> setUserType( detail::FragmentType::CTB ) ;
  fragptr -> setSequenceID( ev_counter() ) ; 
  fragptr -> setFragmentID( fragment_id() ) ; 

  fragptr -> setTimestamp( timestamp ) ;
  TLOG( 20, "TriggerBoardReader") << "fragment created with TS " << timestamp << " containing " << word_counter << " words" << std::endl ;

  if ( ! has_TS ) {
    TLOG( 20, "TriggerBoardReader") << "Created fragment with invalid TimeStamp" << std::endl ;
  }

  return fragptr ;
  
}



void dune::TriggerBoardReader::start() {

  _run_controller -> send_start() ;

}

void dune::TriggerBoardReader::stop() {

  _run_controller -> send_stop() ;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TriggerBoardReader) 
