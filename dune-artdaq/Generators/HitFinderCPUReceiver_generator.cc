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


//
// JSON configuration file write FCL param
// {
// "socket"
// {
// "type" : "SUB",
// "bind": ["tcp://127.0.0.1:40000"]
// }
// }
//
// Constructor ------------------------------------------------------------------------------
dune::HitFinderCPUReceiver::HitFinderCPUReceiver(fhicl::ParameterSet const & ps):
  instance_name_("HitFinderCPUReceiver"),
  timeout_(ps.get<int>("HitFinderTimeout")),
  receiver_config_(ps.get<std::string>("SocketType"),
                   ps.get<std::string>("Attach"),
                   ps.get<std::vector<std::string>>("EndPoints"),
                   ps.get<int>("PTMPReceiverTimeout")),
  receiver_("blablab"),
  sender_("blabla"),
  aggregation_(10)
{
  DAQLogger::LogInfo(instance_name_) << "Done - Configuring the TPReceiver\n";
  DAQLogger::LogInfo(instance_name_) << "Done - Configuring the TPSender\n";
  //request_receiver_ = std::make_unique<RequestReceiver>(requester_address_);

}

// start() routine --------------------------------------------------------------------------
void dune::HitFinderCPUReceiver::start(void)
{
  DAQLogger::LogDebug(instance_name_) << "start() called\n";
  // Setup the PTMP receiver
  //receiver_();
  DAQLogger::LogDebug(instance_name_) << "PTMP Receiver set\n";
    
}


void dune::HitFinderCPUReceiver::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";
  // 
  
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
  if (should_stop()) return true;

  // The hits that are going to be received and send
  std::vector<ptmp::data::TPSet> HitSets;
  size_t n_received = 0;
  size_t times = 0;
  
  while(n_received >= aggregation_) {
    // Call the receiver
    ptmp::data::TPSet SetReceived;
    bool received = receiver_(SetReceived, receiver_config_.timeout);
    if (received) {
      n_received++;
      times = 0;
      HitSets.push_back(SetReceived);
    } else {
      times++;
      usleep(receiver_config_.timeout);
    }
    
    if (n_received >= aggregation_) break;

    if (receiver_config_.timeout * times >= (size_t)timeout_)
      return true;
  }

  // Make a fragment.  Follow the way it is done in the SSP
  // boardreader. We always form fragments, even for the events
  // that we're not going to send out to artdaq (spill
  // start/end, run start), mostly to get the timestamp
  // calculation done in the fragment, but partly to make the
  // code easier to follow(?)
  ptmp::data::TPSet SetToSend;
  SetToSend.set_count(1);
  SetToSend.set_detid(4);

  for (auto const& HitSet: HitSets) {
    
    for (uint32_t it=0; it<HitSet.count(); ++it) { // How do we check the size of the fragment?
      ptmp::data::TrigPrim* ptmp_prim=SetToSend.add_tps();

      ptmp_prim->set_channel(HitSet.tps()[it].channel());
      ptmp::data::Timestamp* ts_out = ptmp_prim->mutable_tstart();
      ptmp::data::Timestamp ts_in = ptmp_prim->tstart();
      ts_out->set_seconds(ts_in.seconds());
      ptmp_prim->set_tspan(HitSet.tps()[it].tspan());
      ptmp_prim->set_adcsum(HitSet.tps()[it].adcsum());
      
      std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes(sizeof(dune::TriggerPrimitive),//HitFragment::size()*sizeof(uint32_t),
                                                                            artdaq::Fragment::InvalidSequenceID,
                                                                            artdaq::Fragment::InvalidFragmentID,
                                                                            artdaq::Fragment::InvalidFragmentType,
                                                                            0);//dune::HitFragment::Metadata(HitFragment::VERSION));
      // It's unclear to me whether the constructor above actually sets the metadata, so let's do it here too to be sure
      // f->updateMetadata(HitFragment::Metadata(HitFragment::VERSION));
      f->updateMetadata(0);//HitFragment::Metadata(HitFragment::VERSION));

      // Fill in the fragment header fields (not some other fragment generators may put these in the
      // constructor for the fragment, but here we push them in one at a time.
      f->setSequenceID(ev_counter());  // ev_counter is in our base class  // or f->setSequenceID(fo.get_evtctr())
      f->setFragmentID(fragment_id()); // fragment_id is in our base class, fhicl sets it
      f->setUserType(dune::detail::CPUHITS);

      DAQLogger::LogInfo(instance_name_) << "For timing fragment with sequence ID " << ev_counter();
      // << ", scmd " << std::showbase << std::hex << fo.get_scmd()
      // << std::dec <<  ", setting the timestamp to "
      // << fo.get_tstamp();

      dune::HitFragment hitfrag(*f);    // Overlay class - the internal bits of the class are
      f->setTimestamp(0);  // not too sure why we need this one again in the body

  
      // (i.e. what if there are too much hits?)
      // TPSet are all different size of uint, no need to cast
      // but need to check whether the values are not too big
      uint16_t channel = UINT16_MAX;
      uint64_t startTime = UINT64_MAX;
      uint16_t charge = UINT16_MAX;
      uint16_t timeOverThreshold = UINT16_MAX;
    
      if(HitSet.tps()[it].channel() < UINT16_MAX) channel = HitSet.tps()[it].channel(); // uint32_t -> uint16_t
      startTime = 0;// HitSet.tps()[it].tstart(); // This one is effectively a cast from uint32_t -> uint64_t
      // (i.e. we don't care about the max value it will always be
      // in smaller than the uint64_t)
      if(HitSet.tps()[it].adcsum() < UINT16_MAX) charge = HitSet.tps()[it].adcsum(); // uint32_t -> uint16_t
      if(HitSet.tps()[it].tspan() < UINT16_MAX) timeOverThreshold = HitSet.tps()[it].tspan(); // uint32_t -> uint16_t
      dune::TriggerPrimitive hit(channel, startTime, charge, timeOverThreshold); // uint32_t -> uint16_t
      hitfrag.get_primitive() = hit; // Finally, fill this fragment with this hit.
      frags.emplace_back(std::move(f));
    }
  }
  // We only increment the event counter for events we send out
  ev_counter_inc();

  return true;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::HitFinderCPUReceiver)
