//################################################################################
//# /*
//#        HitFinderCPUReceiver_generator.cc
//#
//#  PLasorak
//#  Feb 2019
//#  for ProtoDUNE
//# */
//###############################################################################

#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_HitFinderCPUReceiver").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/HitFinderCPUReceiver.hh"
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
dune::HitFinderCPUReceiver::HitFinderCPUReceiver(fhicl::ParameterSet const & ps):
  instance_name_("HitFinderCPUReceiver"),
  receiver_config_(ps.get<std::string>("SocketType"),
                   ps.get<std::string>("Attach"),
                   ps.get<std::vector<str::string>>("EndPoints"),
                   ps.get<int>("Timeout")),
  sender_config_  (ps.get<std::string>("Type"),
                   ps.get<std::string>("EndPoint")),
  receiver_(receiver_config_.DumpJSON()),
  sender_  (sender_config_  .DumpJSON())
{
  DAQLogger::LogInfo(instance_name_) << "Done - Configuring the TPReceiver\n";
  DAQLogger::LogInfo(instance_name_) << "Done - Configuring the TPSender\n";
}

// start() routine --------------------------------------------------------------------------
void dune::HitFinderCPUReceiver::start(void)
{
  DAQLogger::LogDebug(instance_name_) << "start() called\n";
}


void dune::HitFinderCPUReceiver::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";
}


void dune::HitFinderCPUReceiver::stopNoMutex(void)
{
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";
}


bool dune::HitFinderCPUReceiver::checkHWStatus_()
{
  return true;
}


bool dune::HitFinderCPUReceiver::getNext_(artdaq::FragmentPtrs &frags)
{
  std::vector<TriggerPrimitiveFinder::WindowPrimitives> AllThePrimitivesPassed;

  // The hits that are going to be sent.
  ptmp::data::TPSet& tps;

  while (true) {
    // Call the receiver
    bool received = receiver_(tps, receiver_config_.timeout);

    bool sent = 0;

    if (received) {
      // If some data was received send them
      sent = sender_(tps);
    } else {
      // else wait for some time
      usleep(receiver_config_.timeout);
    }

    // Stops when the data has been received and sent.
    if (received && sent) break;
  }
  
  return true;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::HitFinderCPUReceiver)
