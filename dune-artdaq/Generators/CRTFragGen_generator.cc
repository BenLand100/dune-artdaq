/* Author: Matthew Strait <mstrait@fnal.gov> */

#include "dune-artdaq/Generators/CRTFragGen.hh"

#include "canvas/Utilities/Exception.h"

#include "artdaq/Application/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"

#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>
#include "cetlib_except/exception.h"

#include "uhal/uhal.hpp"

CRT::FragGen::FragGen(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps)
  , sqltable(ps.get<std::string>("sqltable", ""))
  , readout_buffer_(nullptr)
  , hardware_interface_(new CRTInterface(ps))
  , timestamp_(0)
  , uppertime(0)
  , oldlowertime(0)
  , runstarttime(0)
  , timingXMLfilename(ps.get<std::string>("connections_file", "/nfs/sw/timing/dev/software/v4a3/timing-board-software/tests/etc/connections.xml"))
  , hardwarename(ps.get<std::string>("hardware_select", "PDTS_SECONDARY"))
{
  hardware_interface_->AllocateReadoutBuffer(&readout_buffer_);

  // Start up Camillo's DAQ here. A 5-10s startup time is acceptable here since
  // the rest of the ProtoDUNE DAQ takes more than that.  Once we start
  // up the backend, files will start piling up, but that's ok since
  // we will have a cron job to clean them up, and we will always read
  // only from the latest file when data is requested.
  //
  // Yes, a call to system() is awful.  We could improve this.
  if(system(("source /nfs/sw/crt/readout_linux/script/setup.sh; "
              "startallboards_shortbaseline.pl " + sqltable).c_str())){
    fprintf(stderr, "Failed to start up CRT backend\n");
    // Maybe instead of exiting here, I'm supposed to set a flag that
    // causes the next call to getNext_ to return false.
    exit(1);
  }

  // We might even take baselines here and process them so that we can
  // do pedestal subtraction online if that is desired.

  // TODO: Start up the timing "pdtbutler" here once Dave gives me the
  // required magic words.
}

CRT::FragGen::~FragGen()
{
  // Stop the backend DAQ.
  if(system(("source /nfs/sw/crt/readout_linux/script/setup.sh; "
              "stopallboards.pl " + sqltable).c_str())){
    fprintf(stderr, "Failed to start up CRT backend\n");
    exit(1);
  }

  hardware_interface_->FreeReadoutBuffer(readout_buffer_);
}


// NOTE: Ok to return up to, say, 10s of old data on start up.
// This gets buffered outside of my code and discarded if it isn't
// wanted.

// We don't have to check if we're keeping up because the artdaq system
// does that for us.
bool CRT::FragGen::getNext_(
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

  // Repair the CRT timestamp.  First get the lower bits that the CRT
  // provides.  Then check if they rolled over and increment our stored
  // upper bits if needed, and finally concatenate the two.
  // XXX: does the CRT data come strictly in time order?  If not, this
  // is broken.
  uint64_t lowertime = 0;
  memcpy(&lowertime + 4, readout_buffer_ + 8, 4);

  if(lowertime < oldlowertime) uppertime++;

  timestamp_ += (uint64_t)uppertime << 32;

  // And also copy the upper bits into the buffer itself.  Not sure
  // which timestamp code downstream is going to read (timestamp_ or
  // from the raw buffer, a.k.a. the fragment), but both will always be
  // the same.
  memcpy(readout_buffer_ + 4, &uppertime, 4);

  std::unique_ptr<artdaq::Fragment> fragptr(
    // See $ARTDAQ_DIR/Data/Fragment.hh
    artdaq::Fragment::FragmentBytes(
      bytes_read,
      ev_counter(), // from base CommandableFragmentGenerator
      fragment_id(), // ditto

      // TODO: Needs to be updated to work with the rest of ProtoDUNE-SP
      artdaq::Fragment::FirstUserFragmentType,

      0, // metadata.  We have none.

      timestamp_
  ));

  // NOTE: we are always returning zero or one fragments, but we
  // could return more at the cost of some complexity, maybe getting
  // an efficiency gain.  Check back here if things are too slow.
  frags.emplace_back(std::move(fragptr));

  memcpy(frags.back()->dataBeginBytes(), readout_buffer_, bytes_read);

  if (metricMan /* What is this? */ != nullptr)
    metricMan->sendMetric("Fragments Sent", ev_counter(), "Events", 3 /* ? */,
        artdaq::MetricMode::LastPoint);

  ev_counter_inc(); // from base CommandableFragmentGenerator

  return true;
}

void CRT::FragGen::getRunStartTime()
{
  std::string filepath = "file://" + timingXMLfilename;
  uhal::ConnectionManager timeConnMan(filepath);
  uhal::HwInterface timinghw(timeConnMan.getDevice(hardwarename));

  uhal::ValWord<uint32_t> starthi =
    timinghw.getNode("master.global.version" /* TODO: read right two registers */).read();
  timinghw.dispatch();
  uhal::ValWord<uint32_t> startlo =
    timinghw.getNode("master.global.version").read();
  timinghw.dispatch();

  runstarttime = ((uint64_t)starthi << 32) + startlo;
}

void CRT::FragGen::start()
{
  hardware_interface_->StartDatataking();

  getRunStartTime();

  uppertime = runstarttime >> 32;
  oldlowertime = runstarttime;
}

void CRT::FragGen::stop()
{
  hardware_interface_->StopDatataking();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(CRT::FragGen)
