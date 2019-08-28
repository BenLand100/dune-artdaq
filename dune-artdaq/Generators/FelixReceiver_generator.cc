
// For an explanation of this class, look at its header,
// FelixReceiver.hh, as well as
// https://cdcvs.fnal.gov/redmine/projects/artdaq-dune/wiki/Fragments_and_FragmentGenerators_w_Toy_Fragments_as_Examples
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/FelixReceiver.hh"

//#include "canvas/Utilities/Exception.h"

#include "artdaq/Generators/GeneratorMacros.hh"

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
  frame_size_(ps.get<size_t>("frame_size")),
  op_mode_(ps.get<std::string>("op_mode", "publish")),
  num_links_(ps.get<unsigned>("num_links", 10)),
  //readout_buffer_(nullptr),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType ))
{
  DAQLogger::LogInfo("dune::FelixReceiver::FelixReceiver")<< "Preparing HardwareInterface for FELIX.";

  if (op_mode_ == "onhost") {
    onhost = true;
    flx_hardware_interface_ = std::unique_ptr<FelixOnHostInterface>( new FelixOnHostInterface(ps) );
    message_size_ = flx_hardware_interface_->MessageSize();
    trigger_window_size_ = flx_hardware_interface_->TriggerWindowSize();
    flx_frag_ids_ = fragmentIDs();
  } else { // Fallback to publisher mode.
    onhost = false;
    netio_hardware_interface_ = std::unique_ptr<FelixHardwareInterface>( new FelixHardwareInterface(ps) );
    message_size_ = netio_hardware_interface_->MessageSize();
    trigger_window_size_ = netio_hardware_interface_->TriggerWindowSize();
  }

  /* ADDITIONAL METADATA IF NEEDED */
  // RS -> These metadata will be cleared out!
  metadata_.num_frames = 0;
  metadata_.reordered = 0;
  metadata_.compressed = 0;
  fragment_type_ = toFragmentType("FELIX");

  // Metrics
  instance_name_for_metrics_ = "FelixReceiver";
  num_frags_m_ = 0;
}

dune::FelixReceiver::~FelixReceiver() {
  DAQLogger::LogInfo("dune::FelixReceiver::~FelixReceiver") << " Destructing FelixReceiver.";
}

bool dune::FelixReceiver::getNext_(artdaq::FragmentPtrs & frags) {
  // RS -> On stop, busy check.
  if (should_stop()) { 
    DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << "should_stop() issued! Check for busy links...";
    if (onhost) {
      std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
      while( flx_hardware_interface_->Busy() ) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
      }
      auto tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
      DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << " TRIGGER MATCHING TOOK " << tdelta.count() << " us";
    } else {
      while ( netio_hardware_interface_->Busy() ) {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
      }
    }
    DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << "No more busy links, returning...";
    return false;

  } else { // Should continue

    if (onhost) {
      // ONHOIST MODE
      std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();    
      for (uint32_t i=0; i<num_links_; ++i){
        std::unique_ptr<artdaq::Fragment> fragptr(
        artdaq::Fragment::FragmentBytes(0, ev_counter(), flx_frag_ids_[i],
                                        fragment_type_, metadata_, timestamp_));
        frags.emplace_back( std::move(fragptr) );
        num_frags_m_++;
      }
      auto tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
      DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << " Preparing empty frags took " << tdelta.count() << " us";
      t1 = std::chrono::high_resolution_clock::now();    
      bool done = false;
      done = flx_hardware_interface_->FillFragments( frags );
      //num_frags_m_ += num_links_; // Already incremented in the frag creation.
      tdelta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - t1);
      DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << " TRIGGER MATCHING TOOK " << tdelta.count() << " us DONE?" << done;
      ev_counter_inc();
      return true;
      // EOF ONHOST MODE
    } else { 
      // PUBLISH MODE
      std::unique_ptr<artdaq::Fragment> fragptr(
        artdaq::Fragment::FragmentBytes(0, ev_counter(), fragment_id(),
                                        fragment_type_, metadata_, timestamp_)
      );
      bool done = false;
      while ( !done ){
        done = netio_hardware_interface_->FillFragment( fragptr );
        if (should_stop()) { return true; } // interrupt data capture and return; at next getNext stopping will be done
      }
      frags.emplace_back( std::move(fragptr) );
      num_frags_m_++;
      ev_counter_inc();
      return true;
      // EOF PUBLISH MODE
    }
 
  }
  return false;
}

void dune::FelixReceiver::start() {
  DAQLogger::LogInfo("dune::FelixReceiver::start") << "Start datatatking for FelixHardwareInterfaces.";
  if (op_mode_ == "onhost") {
    flx_hardware_interface_->StartDatataking();
  } else {
    netio_hardware_interface_->StartDatataking();
  }
}

void dune::FelixReceiver::stop() {
  DAQLogger::LogInfo("dune::FelixReceiver::stop") << "Stop datataking for FelixHardwareInterfaces.";
  if (should_stop()) {
    if (op_mode_ == "onhost") {
      flx_hardware_interface_->StopDatataking();
    } else {
      netio_hardware_interface_->StopDatataking();
    }
  }
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::FelixReceiver)

