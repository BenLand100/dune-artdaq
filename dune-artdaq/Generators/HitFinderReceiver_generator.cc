//################################################################################
//# /*
//#        HitFinderReceiver_generator.cc
//#
//#  PLasorak
//#  Feb 2019
//#  for ProtoDUNE
//# */
//###############################################################################

#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_HitFinderReceiver").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/HitFinderReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"

#include "fhiclcpp/ParameterSet.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <set>

#include <unistd.h>

#include <cstdio>

#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push
#include "pdt/PartitionNode.hpp" // The interface to a timing system
                                 // partition, from the
                                 // protodune_timing UPS product

#pragma GCC diagnostic pop

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

// Constructor ------------------------------------------------------------------------------
dune::HitFinderReceiver::HitFinderReceiver(fhicl::ParameterSet const & ps):
  instance_name_("HitFinderReceiver")
{
  usleep(2000000);
  DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";
}

// start() routine --------------------------------------------------------------------------
void dune::HitFinderReceiver::start(void)
{
  DAQLogger::LogDebug(instance_name_) << "start() called\n";
}


void dune::HitFinderReceiver::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";
}


void dune::HitFinderReceiver::stopNoMutex(void)
{
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";
}


bool dune::HitFinderReceiver::checkHWStatus_()
{
  return true;
}


bool dune::HitFinderReceiver::getNext_(artdaq::FragmentPtrs &frags)
{
  return true;
}


DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::HitFinderReceiver)
