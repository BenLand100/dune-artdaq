
// For an explanation of this class, look at its header,
// ToySimulator.hh, as well as
// https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/wiki/Fragments_and_FragmentGenerators_w_Toy_Fragments_as_Examples

#include "dune-artdaq/Generators/TriggerBoardReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "dune_artdaq/dune-artdaq/Generators/TriggerBoard/content.h"


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
  throw_exception_(ps.get<bool>("throw_exception",false) )
{

  //get port and host from the ps files
  const unsigned int control_port = ps.get<uint16_t>("control_port", 8991 ) ;
  const std::string address = ps.get<std::string>( "board_address", "np04-ctb-1" );

  dune::DAQLogger::LogDebug("TriggerBoardGenerator") << "control messages sent to " << address << ":" << control_port << std::endl; 

  _group_size = ps.get<unsigned int>( "group_size", 1 ) ;
  dune::DAQLogger::LogDebug( "TriggerBoardGenerator") << "Creating fragments with " << _group_size << " packages from the CTB" ;

  //build the ctb_controller 
  _run_controller.reset( new CTB_Controller( address, control_port ) ) ;

  // set it to listen mode to receive contact from ctb 
  const unsigned int receiver_port = ps.get<uint16_t>("receiver_port", 8992 ) ;
  const std::string receiver_address = ps.get<std::string>( "receiver_address", boost::asio::ip::host_name() );
  
  dune::DAQLogger::LogInfo("TriggerBoardGenerator") << "control messages receved at " << receiver_address << ":" << receiver_port << std::endl; 
  
  const unsigned int timeout = ps.get<uint16_t>("receiver_timeout", 10 ) ; //milliseconds

  //build the ctb receiver and set its connecting port
  _receiver.reset( new CTB_Receiver( receiver_port, timeout ) ) ;
 
  if ( ps.has_key( "calibration_stream_output") ) {
    _receiver -> SetCalibrationStream( ps.get<std::string>( "calibration_stream_output"), 
				       std::chrono::minutes( ps.get<unsigned int>( "calibration_update", 5 )  ) ) ;
  }
    
  //configure the ctb controller
  // to instruct it to the receiver
  _run_controller -> send_config( receiver_address, receiver_port, ps.get<unsigned long>( "rollover", 50000 ) ) ;
  
}

dune::TriggerBoardReader::~TriggerBoardReader() {

  //call destructors for ctb_controller and ctb_receiver
  _run_controller -> send_stop() ;

}

bool dune::TriggerBoardReader::getNext_(artdaq::FragmentPtrs & frags) {

  if ( should_stop() ) {
    return false;
  }

  DAQLogger::LogDebug("TriggerBoardReader") << "N TS Words " << _receiver -> N_TS_Words().load() << std::endl ;

  if ( _group_size > 0 ) {
    if ( _receiver -> N_TS_Words().load() < _group_size ) return false ;
  }

  std::size_t n_words = _receiver -> Buffer().read_available() ;

  DAQLogger::LogDebug("TriggerBoardReader") << "N Words " << n_words << std::endl ;

  if ( n_words == 0 ) return false ;
  
  std::size_t bytes_read = n_words * 16 ; 

  bool has_TS = false ;
  artdaq::Fragment::timestamp_t timestamp = artdaq::Fragment::InvalidTimestamp ; 

  if ( _has_last_TS ) {
    has_TS = true ; 
    timestamp = _last_timestamp ;
  }
  
  ptb::content::word::word temp_word  ;

  std::unique_ptr<artdaq::Fragment> fragptr( artdaq::Fragment::FragmentBytes( bytes_read ,  
									      ev_counter(), 
									      fragment_id(),
									      1 ,         //fragment_type_ //what is this?!
									      temp_word ) );  //metadata, necessary if we need to specify a timestamp
					     
  
  unsigned int word_counter = 0 ;
  unsigned int group_counter = 0 ;
  
  for ( unsigned int i = 0 ; i < n_words; ++i ) { 

    _receiver -> Buffer().pop( temp_word ) ;

    // We don't want to send TS words in a fragment
    if ( ! CTB_Receiver::IsTSWord( temp_word ) ) {
      memcpy( fragptr->dataBeginBytes() + word_counter * 16, temp_word.get_bytes(), 16 ) ;
      word_counter++ ;
    }
    else { 
      
      _last_timestamp = temp_word.frame.timestamp ; 
      _has_last_TS = true ;
      
      if ( ! has_TS ) {
	has_TS = true ;
	timestamp = _last_timestamp ; 
      }

      if ( i != 0 ) group_counter++ ;
    }

    if ( _group_size > 0 )
      if ( group_counter >= _group_size ) { 
	DAQLogger::LogInfo("TriggerBoardReader") << "Final group_counter " << group_counter << std::endl ;
	_receiver -> N_TS_Words() -= _group_size ; 
	group_counter = 0 ;
	break ;
      }
    
  }

  fragptr -> setTimestamp( timestamp ) ;

  if ( ! has_TS ) {
    DAQLogger::LogError("TriggerBoardReader") << "Created fragment with invalid TimeStamp" << std::endl ;
  }

  fragptr -> resize( word_counter * 16 ) ;

  frags.emplace_back( std::move(fragptr) ) ;

  DAQLogger::LogInfo("TriggerBoardReader") << "Sending a fragment " << std::endl ;

  // if (ev_counter() % 1 == 0) {

  //   // if(artdaq::Globals::metricMan_ != nullptr) {
  //   //   artdaq::Globals::metricMan_->sendMetric("Fragments Sent",ev_counter(), "Fragments", 0, artdaq::MetricMode::Accumulate);
  //   // }

  //   DAQLogger::LogDebug("TriggerBoardReader") << "On fragment " << ev_counter() << ", this is a test of the DAQLogger's LogDebug function";


  //   if (throw_exception_) {
  //     DAQLogger::LogError("TriggerBoardReader") << "On fragment " << ev_counter() << ", this is a test of the DAQLogger's LogError function";

  //   }
  // }

  return true;
}

void dune::TriggerBoardReader::start() {

  _run_controller -> send_start() ;

}

void dune::TriggerBoardReader::stop() {

  _run_controller -> send_stop() ;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TriggerBoardReader) 
