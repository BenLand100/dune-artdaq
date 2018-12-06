
// For an explanation of this class, look at its header,
// ToySimulator.hh, as well as
// https://cdcvs.fnal.gov/redmine/projects/dune-artdaq/wiki/Fragments_and_FragmentGenerators_w_Toy_Fragments_as_Examples

#include "dune-artdaq/Generators/ToySimulator.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "dune-raw-data/Overlays/ToyFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"

#include "canvas/Utilities/Exception.h"
#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>


dune::ToySimulator::ToySimulator(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  hardware_interface_( new ToyHardwareInterface(ps) ),
  timestamp_(0),
  timestampScale_(ps.get<int>("timestamp_scale_factor", 1)),
  readout_buffer_(nullptr),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType )),
  throw_exception_(ps.get<bool>("throw_exception",false))
{
  hardware_interface_->AllocateReadoutBuffer(&readout_buffer_);   

  metadata_.board_serial_number = hardware_interface_->SerialNumber();
  metadata_.num_adc_bits = hardware_interface_->NumADCBits();

  switch (hardware_interface_->BoardType()) {
  case 1006:
    fragment_type_ = toFragmentType("TOY1");
    break;
  case 1007:
    fragment_type_ = toFragmentType("TOY2");
    break;
  default:
    throw cet::exception("ToySimulator") << "Unable to determine board type supplied by hardware";
  }


}

dune::ToySimulator::~ToySimulator() {
  hardware_interface_->FreeReadoutBuffer(readout_buffer_);
}


bool dune::ToySimulator::checkHWStatus_() {

  //always return true unless some fatal error
  return true; 
}

bool dune::ToySimulator::getNext_(artdaq::FragmentPtrs & frags) {

  if (should_stop()) {
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

  std::size_t bytes_read = 0;
  hardware_interface_->FillBuffer(readout_buffer_ , &bytes_read);

  // We'll use the static factory function 

  // artdaq::Fragment::FragmentBytes(std::size_t payload_size_in_bytes, sequence_id_t sequence_id,
  //  fragment_id_t fragment_id, type_t type, const T & metadata)

  // which will then return a unique_ptr to an artdaq::Fragment
  // object. 

  std::unique_ptr<artdaq::Fragment> fragptr(
   					    artdaq::Fragment::FragmentBytes(bytes_read,  
   									    ev_counter(), fragment_id(),
   									    fragment_type_, 
   									    metadata_,
									    timestamp_));

  memcpy(fragptr->dataBeginBytes(), readout_buffer_, bytes_read );

  frags.emplace_back( std::move(fragptr ));

  if(artdaq::Globals::metricMan_ != nullptr) {
    artdaq::Globals::metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3, artdaq::MetricMode::Accumulate);
  }

  ev_counter_inc();
  timestamp_ += timestampScale_;


  if (ev_counter() % 1 == 0) {

    // if(artdaq::Globals::metricMan_ != nullptr) {
    //   artdaq::Globals::metricMan_->sendMetric("Fragments Sent",ev_counter(), "Fragments", 0, artdaq::MetricMode::Accumulate);
    // }

    DAQLogger::LogDebug("ToySimulator") << "On fragment " << ev_counter() << ", this is a test of the DAQLogger's LogDebug function";


    if (throw_exception_) {
      DAQLogger::LogError("ToySimulator") << "On fragment " << ev_counter() << ", this is a test of the DAQLogger's LogError function";

    }
  }

  return true;
}

void dune::ToySimulator::start() {
  hardware_interface_->StartDatataking();
  timestamp_ = 0;
}

void dune::ToySimulator::stop() {
  hardware_interface_->StopDatataking();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::ToySimulator) 
