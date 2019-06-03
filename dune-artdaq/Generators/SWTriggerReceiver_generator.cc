#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_SWTriggerReceiver").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/SWTriggerReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"
#include "dune-raw-data/Overlays/HitFragment.hh"

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
#include <random>

#include <unistd.h>

#include <cstdio>

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

dune::SetZMQSigHandler::SetZMQSigHandler() {
  setenv("ZSYS_SIGHANDLER", "false", true);
}

// Constructor ------------------------------------------------------------------------------
dune::SWTriggerReceiver::SWTriggerReceiver(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps),
  zmq_sig_handler_(),
  instance_name_("SWTriggerReceiver"),
  must_stop_(0),
  tpsorted_("{\
\"input\": \
{\"socket\": { \"type\": \"SUB\", \"connect\": [ \"tcp://10.73.136.60:15151\", \"tcp://10.73.136.60:15152\" ] } },\
\"output\":\
{\"socket\": { \"type\": \"PUB\", \"bind\": [ \"tcp://*:25441\" ] } },\
\"tardy\": 1000\
}"),
  receiver_("{\"socket\": { \"type\": \"SUB\", \"connect\": [ \"tcp://10.73.136.32:25441\" ] } }"),
  nrecvd_(0)
{
  DAQLogger::LogInfo(instance_name_) << "Initiated SWTriggerReceiver\n";
}

// start() routine --------------------------------------------------------------------------
void dune::SWTriggerReceiver::start(void)
{
}


void dune::SWTriggerReceiver::stop(void)
{
    DAQLogger::LogInfo(instance_name_) << "received " << nrecvd_ << " tpsets";
}


void dune::SWTriggerReceiver::stopNoMutex(void)
{
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";
}


bool dune::SWTriggerReceiver::checkHWStatus_()
{
  return true;
}


bool dune::SWTriggerReceiver::getNext_(artdaq::FragmentPtrs&)
{
    // uint64_t start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  if (should_stop()) return false;

  ptmp::data::TPSet SetReceived;
  bool recvd=receiver_(SetReceived, 100);
  if(recvd) ++nrecvd_;
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  return true;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::SWTriggerReceiver)
