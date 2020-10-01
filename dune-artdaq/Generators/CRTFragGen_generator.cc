/* Author: Matthew Strait <mstrait@fnal.gov> */

#include "dune-artdaq/Generators/CRTFragGen.hh"

#include "canvas/Utilities/Exception.h"

#include "artdaq/Generators/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"

#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>
#include "cetlib_except/exception.h"

dune::CRTFragGen::CRTFragGen(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps)
  , hardware_interface_(new CRTInterface(ps))
  , timestamp_(0)
  , readout_buffer_(nullptr)
{
  // NOTE: Strawman scheme: start up Camillo's DAQ here.  Disadvantage:
  // files will pile up for an arbitrary amount of time. Alternatively,
  // could do it in start() if 5-10s startup time is acceptable there.

  hardware_interface_->AllocateReadoutBuffer(&readout_buffer_);
}

dune::CRTFragGen::~CRTFragGen()
{
  hardware_interface_->FreeReadoutBuffer(readout_buffer_);
}


// NOTE: Ok to return up to, say, 10s of old data on start up.
// This gets buffered outside of my code and discarded if it isn't
// wanted.

// We don't have to check if we're keeping up because the artdaq system
// does that for us.
bool dune::CRTFragGen::getNext_(
  std::list< std::unique_ptr<artdaq::Fragment> > & frags)
{
  if(should_stop()) return false;

  std::size_t bytes_read = 0;
  hardware_interface_->FillBuffer(readout_buffer_, &bytes_read);

  if(bytes_read == 0){
    // Pause for a little bit if we didn't get anything to keep load down.
    usleep(1000);
    return true; // this means "keep taking data"
  }

  assert(sizeof timestamp_ == 8);

  // A module packet must at least have the magic number (1B), hit count
  // (1B), module number (2B) and timestamps (8B).
  const std::size_t minsize = 4 + sizeof(timestamp_);
  if(bytes_read < 4 + sizeof(timestamp_)){
    fprintf(stderr, "Bad result with only %lu < %lu bytes from "
            "CRTInterface::FillBuffer.\n", bytes_read, minsize);
    return false; // means "stop taking data"
  }

  // NOTE: would like if this timestamp ended up being the full 64-bit
  // 50MHz clock by the time it got here, possibly via some repair
  // scheme inside FillBuffer that uses a side channel to get the
  // correspondence between Unix times and 50MHz times.

  // The Unix time stamp concatenated with the 50MHz counter
  memcpy(&timestamp_, readout_buffer_ + 4, sizeof(timestamp_));

  std::unique_ptr<artdaq::Fragment> fragptr(
    // See $ARTDAQ_DIR/Data/Fragment.hh
    artdaq::Fragment::FragmentBytes(
      bytes_read,
      ev_counter(), // from base CommandableFragmentGenerator
      fragment_id(), // ditto

      // Needs to be updated to work with the rest of ProtoDUNE-SP
      dune::detail::CRT,

      0, // metadata.  We have none.

      timestamp_
  ));

  // NOTE: we are always returning zero or one fragments, but we
  // could return more at the cost of some complexity, maybe getting
  // an efficiency gain.  Check back here if things are too slow.
  frags.emplace_back(std::move(fragptr));

  memcpy(frags.back()->dataBeginBytes(), readout_buffer_, bytes_read);

  if (metricMan /* What is this? */ != nullptr)
    metricMan->sendMetric("Fragments Sent", ev_counter(), "Events", 3,
        artdaq::MetricMode::LastPoint);

  ev_counter_inc(); // from base CommandableFragmentGenerator

  return true;
}

void dune::CRTFragGen::start()
{
  hardware_interface_->StartDatataking();
}

void dune::CRTFragGen::stop()
{
  // NOTE: Probably let Camillo's DAQ keep running, and then when we
  // get start() again, we'll start with the current second's file.
  // It is ok to return some old data, as in getNext_ comment.
  hardware_interface_->StopDatataking();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::CRTFragGen)
