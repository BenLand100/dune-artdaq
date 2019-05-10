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
#include <random>

#include <unistd.h>

#include <cstdio>

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

dune::SetZMQSigHandler::SetZMQSigHandler() {
  setenv("ZSYS_SIGHANDLER", "false", true);
}

// Constructor ------------------------------------------------------------------------------
dune::HitFinderCPUReceiver::HitFinderCPUReceiver(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps),
  zmq_sig_handler_(),
  instance_name_("HitFinderCPUReceiver"),
  timeout_(ps.get<int>("timeout")), // ptmp timeout (ms)
  waitretry_(ps.get<int>("waitretry")), // ms it should wait before trying again to receive a ptmp message
  ntimes_retry_(ps.get<size_t>("ntimes_retry")),
  aggregation_(ps.get<int>("ptmp_aggregation")),
  must_stop_(0),
  receiver_socket_(ps.get<std::string>("receiver_socket")),
  sender_socket_(ps.get<std::string>("sender_socket")),
  receiver_(std::string("{\"socket\": { \"type\": \"SUB\", \"connect\": [ \"")+receiver_socket_+std::string("\" ] } }")),
  sender_(std::string("{\"socket\": { \"type\": \"PUB\", \"bind\": [ \"")+sender_socket_+std::string("\" ] } }")),
  nTPHit_received_(0),
  nTPSet_received_(0),
  nTPCHit_sent_(0),
  nTPCSet_sent_(0),
  dummy_mode_(ps.get<bool>("dummy_mode", false)), // Whether to enable this mode
  dummy_wait_(ps.get<int>("dummy_wait", 0)), // Time the BR should wait between the TPSend (msec)
  dummy_nhit_per_tpset_(ps.get<size_t>("dummy_nhit_per_tpset", 20)), // Number of hit sent per tpset (actually send poisson fluctuated around this number).
  nextntime(0),
  nextntimestop(0),
  average_tps_size(),
  average_total_time(),
  average_receive_time(),
  average_send_time(),
  average_1_time(),
  average_2_time(),
  average_3_time(),
  average_4_time()
{
  DAQLogger::LogInfo(instance_name_) << "Initiated HitFinderCPUReceiver\n";
}

// start() routine --------------------------------------------------------------------------
void dune::HitFinderCPUReceiver::start(void)
{
  nextntime =0;
  nextntimestop =0;
}


void dune::HitFinderCPUReceiver::stop(void)
{
  if (!dummy_mode_) {
    DAQLogger::LogInfo(instance_name_) << "stop() called\n"
                                       << "HitFinderCPUReceiver\n"
                                       << " - received: " << nTPHit_received_
                                       << " hits in " << nTPSet_received_ << " TPsets\n"
                                       << " - sent: " << nTPCHit_sent_ << " hits/fragments in "
                                       << nTPCSet_sent_ << " TPsets.";
  } else {
    DAQLogger::LogInfo(instance_name_) << "stop() called\n"
                                       << "HitFinderCPUReceiver\n"
                                       << " - received: " << nTPHit_received_
                                       << " hits in " << nTPSet_received_ << " TPsets (these two should be 0 as the BR ran in dummy mode)\n"
                                       << " - sent: " << nTPCHit_sent_ << " hits/fragments in "
                                       << nTPCSet_sent_ << " TPsets.";
  }
  DAQLogger::LogInfo(instance_name_) << "Number of times GetNEXT was called " << nextntime << "\n";
  DAQLogger::LogInfo(instance_name_) << "Number of times GetNEXT was called stop " << nextntimestop << "\n";
  double mean=0;
  for (auto const& it: average_tps_size) {
    mean += it;
  }
  mean /= average_tps_size.size();
  DAQLogger::LogInfo(instance_name_) << "Average tps_size received (last 1000): " << mean << "\n";
  must_stop_ = 1;


  mean=0;
  for (auto const& it: average_total_time) {
    mean += it;
  }
  mean /= average_total_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average getnext time (last 10000): " << mean << "\n";
  
  mean=0;
  for (auto const& it: average_receive_time) {
    mean += it;
  }
  mean /= average_receive_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average PTMP receiving loop time (last 10000): " << mean << "\n";

  mean=0;
  for (auto const& it: average_send_time) {
    mean += it;
  }
  mean /= average_send_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average PTMP sending time (last 10000): " << mean << "\n";
 
  mean=0;
  for (auto const& it: average_1_time) {
    mean += it;
  }
  mean /= average_1_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average 1 time (last 10000): " << mean << "\n";
  
  mean=0;
  for (auto const& it: average_2_time) {
    mean += it;
  }
  mean /= average_2_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average 2 time (last 10000): " << mean << "\n";
  
  mean=0;
  for (auto const& it: average_3_time) {
    mean += it;
  }
  mean /= average_3_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average 3 time (last 10000): " << mean << "\n";
  
  mean=0;
  for (auto const& it: average_4_time) {
    mean += it;
  }
  mean /= average_4_time.size();
  DAQLogger::LogInfo(instance_name_) << "Average 4 time (last 10000): " << mean << "\n"; 
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
  uint64_t start = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  ++nextntimestop;
  if (should_stop()) return false;
  ++nextntime;

  // The hits that are going to be received and send
  std::vector<ptmp::data::TPSet> HitSets;
  size_t n_received = 0;
  size_t times = 0;

  if (!dummy_mode_) {

    while(n_received < aggregation_ && !must_stop_) {
      uint64_t clock0 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

      // Call the receiver
      ptmp::data::TPSet SetReceived;
      uint64_t clock1 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

      bool recved = receiver_(SetReceived, timeout_);
      uint64_t clock2 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();


      times++;

      uint64_t clock3 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
      if (recved) {
        nTPSet_received_++;
        nTPHit_received_+=SetReceived.tps_size();

        // if (average_tps_size.size() < 1000)
        //   average_tps_size.push_back(SetReceived.tps_size());
        // else
        //   average_tps_size.clear();
        
        n_received += (SetReceived.tps_size()>0);
        times = 0;
        HitSets.push_back(SetReceived);

      } else {
        if (must_stop_) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(waitretry_));
      }
      uint64_t clock4 = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

      if (times >= ntimes_retry_) {
        break;
      }

      if (average_1_time.size() < 10000) { average_1_time.push_back(clock1 - clock0); }
      else                               { average_1_time.clear();                    }
      
      if (average_2_time.size() < 10000) { average_2_time.push_back(clock2 - clock1); }
      else                               { average_2_time.clear();                    }
      
      if (average_3_time.size() < 10000) { average_3_time.push_back(clock3 - clock2); }
      else                               { average_3_time.clear();                    }
      
      if (average_4_time.size() < 10000) { average_4_time.push_back(clock4 - clock3); }
      else                               { average_4_time.clear();                    }
    

    }
        
   
  
    if (n_received == 0) return true;
    
  } else {
    
    ptmp::data::TPSet SetReceived;

    // Generate a random poisson number around the value from the fcl
    std::default_random_engine generator;
    std::normal_distribution<double> distribution((double)dummy_nhit_per_tpset_,sqrt((double)dummy_nhit_per_tpset_));
    size_t nhits = 0;
    while (nhits<=1) { // At least 2 (from Phil)
      nhits = (size_t)distribution(generator);
    }
    // Fill the dummy received tpset
    while (nhits>0) {
      ptmp::data::TrigPrim* ptmp_prim=SetReceived.add_tps();
      std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
      //                                       number of ticks per second for a 50MHz clock
      auto ticks = std::chrono::duration_cast<std::chrono::duration<int, std::ratio<1,50000000>>>(now.time_since_epoch());
      SetReceived.set_created(ticks.count());
      ptmp_prim->set_channel(nTPSet_received_);
      ptmp_prim->set_tstart (ticks.count());
      ptmp_prim->set_tspan  (50-nhits);
      ptmp_prim->set_adcsum (nTPSet_received_);
      ++nTPSet_received_;
      --nhits;
    }
    HitSets.push_back(SetReceived);
  }

  uint64_t received = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

  ptmp::data::TPSet SetToSend;
  std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
  //                                       number of ticks per second for a 50MHz clock
  auto ticks = std::chrono::duration_cast<std::chrono::duration<int, std::ratio<1,50000000>>>(now.time_since_epoch());
  SetToSend.set_created(ticks.count());
  SetToSend.set_count(nTPCSet_sent_);
  SetToSend.set_detid(4);
  
  uint64_t minStartTime = UINT64_MAX;
  uint64_t maxStartTime = 0;
  uint32_t minChan = UINT32_MAX;
  uint32_t maxChan = 0;
  uint32_t adcSum = 0;
  
  for (auto const& HitSet: HitSets) {

    for (int it=0; it<HitSet.tps_size(); ++it) { // How do we check the size of the fragment?
      ptmp::data::TrigPrim* ptmp_prim=SetToSend.add_tps();

      ptmp_prim->set_channel(HitSet.tps()[it].channel());
      ptmp_prim->set_tstart (HitSet.tps()[it].tstart ());
      ptmp_prim->set_tspan  (HitSet.tps()[it].tspan  ());
      ptmp_prim->set_adcsum (HitSet.tps()[it].adcsum ());

      std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes(sizeof(dune::TriggerPrimitive),
                                                                            ev_counter(),
                                                                            fragment_id(),
                                                                            dune::detail::CPUHITS,
                                                                            dune::HitFragment::Metadata(HitFragment::VERSION));
      // It's unclear to me whether the constructor above actually sets
      // the metadata, so let's do it here too to be sure
      f->updateMetadata(HitFragment::Metadata(HitFragment::VERSION));
      
      // Fill in the fragment header fields (not some other fragment
      // generators may put these in the
      // constructor for the fragment, but here we push them in one at a time.

      /// Comment from Kurt on slack:
      // It's true that, in window mode, the sequence ID of the Container
      // fragment is set to the value that is sent in the Data Request from the EBs,
      // so whatever you put into the individual fragments for the sequence ID will
      // overwritten.
      // However, I would suggest that you still set this field in the fragments
      // that you are creating, but as you say, it does not have to match up with
      // anything else in the system - it can just be an internal counter value
      f->setSequenceID(ev_counter());
      f->setFragmentID(fragment_id()); // fragment_id is in our base class, fhicl sets it
      f->setUserType(dune::detail::CPUHITS);

      dune::HitFragment hitfrag(*f);
      f->setTimestamp(HitSet.tps()[it].tstart());
      // not too sure why we need this one again here...

      // TPSet are all different size of uint, no need to cast
      // but need to check whether the values are not too big
      uint16_t channel           = UINT16_MAX;
      uint64_t startTime         = UINT64_MAX;
      uint16_t charge            = UINT16_MAX;
      uint16_t timeOverThreshold = UINT16_MAX;
      if(HitSet.tps()[it].channel() < UINT16_MAX) channel           = HitSet.tps()[it].channel(); // uint32_t -> uint16_t
      if(HitSet.tps()[it].adcsum()  < UINT16_MAX) charge            = HitSet.tps()[it].adcsum (); // uint32_t -> uint16_t
      if(HitSet.tps()[it].tspan ()  < UINT16_MAX) timeOverThreshold = HitSet.tps()[it].tspan  (); // uint32_t -> uint16_t
      startTime = HitSet.tps()[it].tstart();

      dune::TriggerPrimitive hit(startTime, channel, 0, charge, timeOverThreshold); // uint32_t -> uint16_t
      
      if (minStartTime > startTime) minStartTime = startTime;
      if (maxStartTime < startTime) maxStartTime = startTime;
      if (minChan > channel) minChan = channel;
      if (maxChan < channel) maxChan = channel;
      adcSum += charge;
  
      hitfrag.get_primitive() = hit; // Finally, fill this fragment with this hit.
      frags.emplace_back(std::move(f));
    }
  }
  
  SetToSend.set_tstart  (minStartTime);
  SetToSend.set_tspan   (maxStartTime-minStartTime);
  SetToSend.set_chanbeg (minChan);
  SetToSend.set_chanend (maxChan);
  SetToSend.set_totaladc(adcSum);

  if (SetToSend.tps_size() > 0) {
    sender_(SetToSend);
    nTPCSet_sent_++;
    nTPCHit_sent_ += SetToSend.tps_size();
  }
  // We only increment the event counter for events we send out
  ev_counter_inc();
  
  // Wait here in dummy mode
  if (dummy_mode_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(dummy_wait_));
  }
  
  uint64_t sent = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
  
  if (average_total_time  .size() < 10000) { average_total_time.push_back(sent - start);       }
  else                                     { average_total_time.clear();                       }

  if (average_send_time   .size() < 10000) { average_send_time.push_back(sent - received);     }
  else                                     { average_send_time.clear();                        }


  
  return true;
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::HitFinderCPUReceiver)
