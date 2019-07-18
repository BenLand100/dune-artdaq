
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
  //readout_buffer_(nullptr),
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType ))
{
  DAQLogger::LogInfo("dune::FelixReceiver::FelixReceiver")<< "Preparing HardwareInterface for FELIX.";
  netio_hardware_interface_ = std::unique_ptr<FelixHardwareInterface>( new FelixHardwareInterface(ps) );
  message_size_ = netio_hardware_interface_->MessageSize();
  trigger_window_size_ = netio_hardware_interface_->TriggerWindowSize();

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
  // RS -> On stop signal, busy check.
  if (should_stop()) { 
    DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << "should_stop() issued! Check for busy links...";
    while ( netio_hardware_interface_->Busy() ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << "No more busy links, returning...";
    return false;
  } else {
    // RS -> This shouldn't be here... FelixReceiver is only responsible for ArtDAQ logic.
    //    -> Move this part to the HWInterface.
    //std::size_t bytes_to_read = frame_size_ * trigger_window_size_;
    //eg180601 specify bytes to read in msgsize
    //GLM: create empty fragment!
    std::unique_ptr<artdaq::Fragment> fragptr(
      artdaq::Fragment::FragmentBytes(0, ev_counter(), fragment_id(),
                                      fragment_type_, metadata_, timestamp_)
    );

    //std::unique_ptr<artdaq::Fragment> fragptr(ev_counter(), fragment_id(),fragment_type_);


    // RS -> This will be the logic to check if there are requests or not. 
    //    -> If FillFragment returns false, it will mean that there are no requests available for some time.
    //       -> Valid to retry FillFragment, if there is NO should_stop issued!
    //    -> A bool/enum parameter, passed to the function will indicate that there was a trigger matching problem.
    //       -> Need to be handled, and understood if it should return false or true from the getNext_
    bool done = false;
    while ( !done ){
      done = netio_hardware_interface_->FillFragment( fragptr );
      if (should_stop()) { return true; } // interrupt data capture and return; at next getNext stopping will be done
    }

    //uint64_t newTS = *(reinterpret_cast<const uint_fast64_t*>(fragptr->dataBeginBytes()+8));
    
    // RS -> Place for debugging purposes to check Fragment consistency, meta, etc..
    //DAQLogger::LogInfo("dune::FelixReceiver::getNext_") << "Passing frag ID " << fragptr->sequenceID()
    //  << " TS: " << fragptr->timestamp();

    //dune::FelixFragmentBase::Metadata metaAfter = *(fragptr->metadata<dune::FelixFragmentBase::Metadata>());
    //uint32_t nf = metaAfter.num_frames;
    //uint32_t re = metaAfter.reordered;
    //uint32_t co = metaAfter.compressed;
    //DAQLogger::LogInfo("dune::FelixReceiver::getNext_")
    //  << "Metadata AFTER update, right before passing.: numFrames:" << (unsigned)nf
    //  << " reordered:" << (unsigned)re
    //  << " compressed:" << (unsigned)co;


    frags.emplace_back( std::move(fragptr) );
    /* RS -> Add metric manager...
    if(artdaq::Globals::metricMan_ != nullptr) {
      artdaq::Globals::metricMan_->sendMetric("Fragments Sent", ev_counter(), "fragments", 1, artdaq::MetricMode::Accumulate);
      // artdaq::Globals::metricMan_->sendMetric("PTB Empty Buffer Low Water Mark", empty_buffer_low_mark_, "buffers", 1, artdaq::MetricMode::Accumulate);
      //artdaq::Globals::metricMan_->sendMetric("Fragment Count", num_frags_m_, "fragments", 1, artdaq::MetricMode::Accumulate);
    }
    */

    num_frags_m_++;
    ev_counter_inc();
    return true;
  }
}

void dune::FelixReceiver::start() {
  DAQLogger::LogInfo("dune::FelixReceiver::start") << "Start datatatking for FelixHardwareInterfaces.";
  netio_hardware_interface_->StartDatataking();
}

void dune::FelixReceiver::stop() {
  DAQLogger::LogInfo("dune::FelixReceiver::stop") << "Stop datataking for FelixHardwareInterfaces.";
  if (should_stop()){
    netio_hardware_interface_->StopDatataking();
  }
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::FelixReceiver)

