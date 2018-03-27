
// For an explanation of this class, look at its header,
// FelixReceiver.hh, as well as
// https://cdcvs.fnal.gov/redmine/projects/artdaq-dune/wiki/Fragments_and_FragmentGenerators_w_Toy_Fragments_as_Examples
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/FelixReceiver.hh"

//#include "canvas/Utilities/Exception.h"

#include "artdaq/Application/GeneratorMacros.hh"

#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

dune::FelixReceiver::FelixReceiver(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  timestamp_(0),
  timestampScale_(ps.get<int>("timestamp_scale_factor", 1)),
  frame_size_(ps.get<size_t>("frame_size", 1024)),
  //readout_buffer_(nullptr),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType ))
{
  DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::FelixReceiver") 
    << "Preparing HardwareInterface for FELIX.";
  netio_hardware_interface_ = std::unique_ptr<FelixHardwareInterface>( new FelixHardwareInterface(ps) );

  /* ADDITIONAL METADATA IF NEEDED */
  // RS -> These metadata will be cleared out!
  metadata_.board_serial_number = 999;
  metadata_.num_adc_bits = 12;
  fragment_type_ = toFragmentType("FELIX");
}

dune::FelixReceiver::~FelixReceiver() {
  DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::~FelixReceiver") << " Destructing FelixReceiver.";
}

bool dune::FelixReceiver::getNext_(artdaq::FragmentPtrs & frags) {
  // RS -> On stop signal, busy check.
  if (should_stop()) {
    
    //DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::getNext_") << "should_stop() issued! Check for busy links...";
    while ( netio_hardware_interface_->Busy() ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    //DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::getNext_") << "No more busy links, returning...";
    return true;
  } else {
    std::size_t bytes_read = 512*10000;  
    std::unique_ptr<artdaq::Fragment> fragptr(
      artdaq::Fragment::FragmentBytes(bytes_read, ev_counter(), fragment_id(),
                                      fragment_type_, metadata_, timestamp_)
    );
    netio_hardware_interface_->FillFragment( fragptr );
    uint64_t newTS = *(reinterpret_cast<const uint_fast64_t*>(fragptr->dataBeginBytes()+8));
    DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::getNext_") << "Before returning, peak to first timestamp:\n"
      << " TS: " << std::hex << newTS << std::dec;

    frags.emplace_back( std::move(fragptr) );
    ev_counter_inc();
    return true;
  }

  // RS -> Check for fragment quality for debugging:
  //std::cout<<"WOOF WOOF -> GENERATOR: getNext() timestamp: "<<std::hex<<"0x"<<timestamp_<<std::dec
  //         <<" sequence: "<<ev_counter()<<std::endl;

  // RS -> Emplace new fragment to Frag container.


  // RS -> Add metric manager...
  //if(metricMan != nullptr) {
  //  metricMan->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  //}

  //ev_counter_inc();
  //return true;
}

void dune::FelixReceiver::start() {
  DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::start()") << "Start datatatking for FelixHardwareInterfaces.";
  netio_hardware_interface_->StartDatataking();
}

void dune::FelixReceiver::stop() {
  DAQLogger::LogInfo("FelixReceiver dune::FelixReceiver::stop()") << "Stop datataking for FelixHardwareInterfaces.";
  if (should_stop()){
    netio_hardware_interface_->StopDatataking();
  }
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::FelixReceiver)

