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
  , partition_number(ps.get<int>("partition_number"))
  , startbackend(ps.get<bool>("startbackend"))
  , timingXMLfilename(ps.get<std::string>("connections_file",
    "/nfs/sw/control_files/timing/connections_v4b4.xml"))
  , hardwarename(ps.get<std::string>("hardware_select", "CRT_EPT"))
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
    throw cet::exception("CRT") << "Failed to start up CRT backend\n";
  }

  // If we aren't the process that starts the backend, this will block
  // until the baselines are available.
  hardware_interface_->SetBaselines();

  // TODO: Start up the timing "pdtbutler" here once Alessandro gives me the
  // required magic words later on 2018-08-27, we hope.
}

CRT::FragGen::~FragGen()
{
  // Stop the backend DAQ.
  if(system(("source /nfs/sw/crt/readout_linux/script/setup.sh; "
              "stopallboards.pl " + sqltable).c_str())){
    TLOG(TLVL_WARNING, "CRT") << "Failed in call to stopallboards.pl\n";
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
    TLOG(TLVL_INFO, "CRT") << "CRT getNext_ returning on should_stop()\n";
    return false;
  }

  std::size_t bytes_read = 0;
  hardware_interface_->FillBuffer(readout_buffer_, &bytes_read);

  if(bytes_read == 0){
    // Pause for a little bit if we didn't get anything to keep load down.
    usleep(10000);

    TLOG(TLVL_DEBUG, "CRT") << "CRT getNext_ is returning with no data\n";
    return true; // this means "keep taking data"
  }

  assert(sizeof timestamp_ == 8);

  // A module packet must at least have the magic number (1B), hit count
  // (1B), module number (2B) and timestamps (8B).
  const std::size_t minsize = 4 + sizeof(timestamp_);
  if(bytes_read < minsize){
    throw cet::exception("CRT") << "Bad result with only " << bytes_read
       << " < " << minsize << " bytes from CRTInterface::FillBuffer.\n";
  }

  // Repair the CRT timestamp.  First get the lower bits that the CRT
  // provides.  Then check if they rolled over and increment our stored
  // upper bits if needed, and finally concatenate the two.
  // This works because the CRT data comes strictly time ordered in
  // each USB stream [citation: Camillo 2018-08-03].
  uint64_t lowertime = 0;
  memcpy(&lowertime + 4, readout_buffer_ + 4 + sizeof(uint64_t), 4);

  // Store the first non-zero raw hardware timestamp that we see.  This is
  // only needed for debugging.  If the first timestamp happens to be zero,
  // in principle we could take that, but it's not important.
  if(firstlowertime == 0) firstlowertime = lowertime;

  // This only works if the CRT takes data continuously.  If we don't
  // see data for more than one 32-bit epoch, which is about 86 seconds,
  // we'll fall out of sync.  With additional work this could be fixed,
  // since we leave the backend DAQ running during pauses, but at the
  // moment we skip over intermediate data when we unpause (we start
  // with the most recent file from the backend, where files are about
  // 5 seconds long).
  if(lowertime < oldlowertime) uppertime++;
  oldlowertime = lowertime;

  timestamp_ = ((uint64_t)uppertime << 32) + lowertime + runstarttime;

  // And also copy the repaired timestamp into the buffer itself.  Not sure
  // which timestamp code downstream is going to read (timestamp_ or
  // from the raw buffer, a.k.a. the fragment), but both will always be
  // the same.
  memcpy(readout_buffer_ + 4, &timestamp_, sizeof(uint64_t));

  // See $ARTDAQ_CORE_DIR/artdaq-core/Data/Fragment.hh, also the "The
  // artdaq::Fragment interface" section of
  // https://cdcvs.fnal.gov/redmine/projects/artdaq-demo/wiki/How_to_write_an_overlay_class

  std::unique_ptr<artdaq::Fragment> fragptr
    = artdaq::Fragment::FragmentBytes(bytes_read);

  // ev_counter() from base CommandableFragmentGenerator
  fragptr->setSequenceID( ev_counter() );
  fragptr->setFragmentID( fragment_id() ); // Ditto
  fragptr->setUserType( dune::detail::CRT );
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

  TLOG(TLVL_DEBUG, "CRT") << "CRT getNext_ is returning with hits\n";

  return true;
}

void CRT::FragGen::getRunStartTime()
{
  std::string filepath = "file://" + timingXMLfilename;
  uhal::setLogLevelTo(uhal::Error());
  static uhal::ConnectionManager timeConnMan(filepath);
  static uhal::HwInterface timinghw(timeConnMan.getDevice(hardwarename));

  // Tell the timing board what partition we are running in.
  // It's ok to do this in all four CRT processes.
  timinghw.getNode("endpoint0.csr.ctrl.tgrp").write(partition_number);
  timinghw.dispatch();

  uhal::ValWord<uint32_t> status
    = timinghw.getNode("endpoint0.csr.stat.ep_stat").read();
  timinghw.dispatch();
  if(status != 8){
    throw cet::exception("CRT") << "CRT timing board in bad state, 0x"
      << std::hex << (int)status << ", can't read run start time\n";
  }

  uhal::ValWord<uint32_t> rst_l = timinghw.getNode("endpoint0.pulse.ts_l").read();
  timinghw.dispatch();
  uhal::ValWord<uint32_t> rst_h = timinghw.getNode("endpoint0.pulse.ts_h").read();
  timinghw.dispatch();

  runstarttime = ((uint64_t)rst_h << 32) + rst_l;

  TLOG(TLVL_INFO, "CRT") << "CRT got run start time " << runstarttime << "\n";
  printf("CRT got run start time 0x%lx\n", runstarttime);
}

void CRT::FragGen::start()
{
  hardware_interface_->StartDatataking();

  getRunStartTime();

  uppertime = 0;
  oldlowertime = 0;
}

void CRT::FragGen::stop()
{
  hardware_interface_->StopDatataking();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(CRT::FragGen)
