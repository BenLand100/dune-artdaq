
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
  readout_buffer_(nullptr),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType ) ),
  throw_exception_(ps.get<bool>("throw_exception",false) )
{

  //get port and host from the ps files
  const unsigned int control_port = ps.get<uint16_t>("control_port", 8991 ) ;
  const std::string address = ps.get<std::string>( "board_address", "np04-ctb-1" );

  dune::DAQLogger::LogDebug("TriggerBoardGenerator") << "control messages sent to " << address << ":" << control_port << std::endl; 

  //build the ctb_controller 
  _run_controller.reset( new CTB_Controller( address, control_port ) ) ;

  
  
  // set it to listen mode to receive contact from ctb 
  const unsigned int receiver_port = ps.get<uint16_t>("receiver_port", 8992 ) ;
  const std::string receiver_address = ps.get<std::string>( "receiver_address", boost::asio::ip::host_name() );
  
  dune::DAQLogger::LogInfo("TriggerBoardGenerator") << "control messages receved at " << receiver_address << ":" << receiver_port << std::endl; 
  
  const unsigned int timeout = ps.get<uint16_t>("receiver_timeout", 10 ) ; //milliseconds

  //build the ctb receiver and set its connecting port
  _receiver.reset( new CTB_Receiver( receiver_port, timeout ) ) ;
  
  //configure the ctb controller
  // to instruct it to the receiver
  _run_controller -> send_config( receiver_address, receiver_port ) ;
  
}

dune::TriggerBoardReader::~TriggerBoardReader() {

  //call destructors for ctb_controller and ctb_receiver
  _run_controller -> send_stop() ;

}

bool dune::TriggerBoardReader::getNext_(artdaq::FragmentPtrs & frags) {

  if ( should_stop() ) {
    return false;
  }

  // ToyHardwareInterface (an instance to which "hardware_interface_"
  // is a unique_ptr object) is just one example of the sort of
  // interface a hardware library might offer. For example, other
  // interfaces might require you to allocate and free the memory used
  // to store hardware data in your generator using standard C++ tools
  // (rather than via the "AllocateReadoutBuffer" and
  // "FreeReadoutBuffer" functions provided here), or could have a
  // function which directly returns a pointer to the data buffer
  // rather than sticking the data in the location pointed to by your
  // pointer (which is what happens here with readout_buffer_)

  

  std::size_t n_words = _receiver -> Buffer().read_available() ;

  if ( n_words == 0 ) return false ;



  std::size_t bytes_read = n_words * 16 ; //  + 4 ; if we need a header, then we add the 4

  ptb::content::word::word temp_word ;

  _receiver -> Buffer().pop( temp_word ) ;
  uint64_t timestamp = temp_word.frame.timestamp ; 
  
  std::unique_ptr<artdaq::Fragment> fragptr( artdaq::Fragment::FragmentBytes( bytes_read ,  
									      ev_counter(), 
									      fragment_id(),
									      1 ,         //fragment_type_ //what is this?!
									      temp_word,  //metadata, necessary if we need to specify a timestamp
									      timestamp ) );   //which time stamp? we have loads! 

  memcpy( fragptr->dataBeginBytes(), temp_word.get_bytes(), 16 ) ;

						      
  for ( unsigned int i = 1 ; i < n_words ; ++i ) {

    _receiver -> Buffer().pop( temp_word ) ;
    memcpy( fragptr->dataBeginBytes() + i * 16, temp_word.get_bytes(), 16 ) ;
    
  }

  frags.emplace_back( std::move(fragptr) ) ;

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
