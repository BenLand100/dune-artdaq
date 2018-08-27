/* Author: Matthew Strait <mstrait@fnal.gov> */

#include "dune-artdaq/Generators/CRTFragGen.hh"

#include "canvas/Utilities/Exception.h"

#include "dune-raw-data/Overlays/FragmentType.hh"

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

#include "artdaq/DAQdata/Globals.hh"

CRT::FragGen::FragGen(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps)
  , sqltable(ps.get<std::string>("sqltable", ""))
  , readout_buffer_(nullptr)
  , hardware_interface_(new CRTInterface(ps))
  , timestamp_(0)
  , uppertime(0)
  , oldlowertime(0)
  , runstarttime(0)
  , startbackend(ps.get<bool>("startbackend"))
  , timingXMLfilename(ps.get<std::string>("connections_file", "/nfs/sw/timing/dev/software/v4a3/timing-board-software/tests/etc/connections.xml"))
  , hardwarename(ps.get<std::string>("hardware_select", "PDTS_SECONDARY"))
{
  hardware_interface_->AllocateReadoutBuffer(&readout_buffer_);

  // If we are the designated process to do so, start up Camillo's backend DAQ
  // here. A 5-10s startup time is acceptable since the rest of the ProtoDUNE
  // DAQ takes more than that.  Once we start up the backend, files will start
  // piling up, but that's ok since we will have a cron job to clean them up,
  // and we will always read only from the latest file when data is requested.
  //
  // Yes, a call to system() is awful.  We could improve this.
  if(startbackend &&
     system(("source /nfs/sw/crt/readout_linux/script/setup.sh; "
              "startallboards_shortbaseline.pl " + sqltable).c_str())){
    TLOG(TLVL_ERROR, "CRT") << "Failed to start up CRT backend\n";
    // TODO: Maybe instead of exiting here, I'm supposed to set a flag that
    // causes the next call to getNext_ to return false.  In general, I don't
    // know how one is supposed to respond to errors inside artdaq.
    exit(1);
  }

  // If we aren't the process that starts the backend, this will block
  // until the baselines are available.
  hardware_interface_->SetBaselines();

  // TODO: Start up the timing "pdtbutler" here once Dave gives me the
  // required magic words.
}

CRT::FragGen::~FragGen()
{
  // Stop the backend DAQ.
  if(system(("source /nfs/sw/crt/readout_linux/script/setup.sh; "
              "stopallboards.pl " + sqltable).c_str())){
    TLOG(TLVL_ERROR, "CRT") << "Failed to start up CRT backend\n";
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
  if(should_stop()){
    // XXX debugging
    TLOG(TLVL_INFO, "CRT") << "CRT getNext_ returning because should_stop() is true\n";
    return false;
  }

  std::size_t bytes_read = 0;
  hardware_interface_->FillBuffer(readout_buffer_, &bytes_read);

  if(bytes_read == 0){
    // Pause for a little bit if we didn't get anything to keep load down.
    usleep(1000);

    // XXX debugging
    TLOG(TLVL_INFO, "CRT") << "CRT getNext_ is returning with no data\n";
    return true; // this means "keep taking data"
  }

  assert(sizeof timestamp_ == 8);

  // A module packet must at least have the magic number (1B), hit count
  // (1B), module number (2B) and timestamps (8B).
  const std::size_t minsize = 4 + sizeof(timestamp_);
  if(bytes_read < 4 + sizeof(timestamp_)){
    TLOG(TLVL_WARNING, "CRT") << "Bad result with only " << bytes_read << " < " << minsize << " bytes from CRTInterface::FillBuffer.\n";
    throw cet::exception("CRT") << "Bad result with only " << bytes_read << " < " << minsize << " bytes from CRTInterface::FillBuffer.\n";
  }

  // Repair the CRT timestamp.  First get the lower bits that the CRT
  // provides.  Then check if they rolled over and increment our stored
  // upper bits if needed, and finally concatenate the two.
  // This works because the CRT data comes strictly time ordered in
  // each USB stream [citation: Camillo 2018-08-03].
  uint64_t lowertime = 0;
  memcpy(&lowertime + 4, readout_buffer_ + 8, 4);

  // This only works if the CRT takes data continuously.  If we don't
  // see data for more than one 32-bit epoch, which is about 86 seconds,
  // we'll fall out of sync.  With additional work this could be fixed,
  // since we leave the backend DAQ running during pauses, but at the
  // moment we skip over intermediate data when we unpause (we start
  // with the most recent file from the backend, where files are about
  // 5 seconds long).
  if(lowertime < oldlowertime) uppertime++;

  timestamp_ += (uint64_t)uppertime << 32;

  // And also copy the upper bits into the buffer itself.  Not sure
  // which timestamp code downstream is going to read (timestamp_ or
  // from the raw buffer, a.k.a. the fragment), but both will always be
  // the same.
  memcpy(readout_buffer_ + 4, &uppertime, 4);

  // See $ARTDAQ_CORE_DIR/artdaq-core/Data/Fragment.hh, also the "The
  // artdaq::Fragment interface" section of
  // https://cdcvs.fnal.gov/redmine/projects/artdaq-demo/wiki/How_to_write_an_overlay_class

  std::unique_ptr<artdaq::Fragment> fragptr = std::make_unique<artdaq::Fragment>(bytes_read);
  fragptr->setSequenceID( ev_counter() ); // ev_counter() from base CommandableFragmentGenerator
  fragptr->setFragmentID( fragment_id() ); // Ditto
  fragptr->setFragmentType( dune::detail::CRT );
  fragptr->setTimestamp( timestamp_ );

  // NOTE: we are always returning zero or one fragments, but we
  // could return more at the cost of some complexity, maybe getting
  // an efficiency gain.  Check back here if things are too slow.
  frags.emplace_back(std::move(fragptr));

  memcpy(frags.back()->dataBeginBytes(), readout_buffer_, bytes_read);


  if (metricMan /* What is this? */ != nullptr)
    metricMan->sendMetric("Fragments Sent", ev_counter(), "Events", 3 /* ? */,
        artdaq::MetricMode::LastPoint);

  ev_counter_inc(); // from base CommandableFragmentGenerator

  // XXX debugging
  TLOG(TLVL_INFO, "CRT") << "CRT getNext_ is returning with hits from a module\n";

  return true;
}

void CRT::FragGen::getRunStartTime()
{
#if 0
  std::string filepath = "file://" + timingXMLfilename;
  uhal::ConnectionManager timeConnMan(filepath);
  uhal::HwInterface timinghw(timeConnMan.getDevice(hardwarename));

  uhal::ValWord<uint32_t> starthi =
    timinghw.getNode("master_top.master.tstamp.ctr" /* TODO: read right two registers */).read();
  timinghw.dispatch();
  uhal::ValWord<uint32_t> startlo =
    timinghw.getNode("master_top.master.tstamp.ctr").read();
  timinghw.dispatch();

  runstarttime = ((uint64_t)starthi << 32) + startlo;
#endif

  runstarttime = 0;
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
