//################################################################################
//# /*
//#        Candidate_generator.cc
//#        Borrowed heavily from SWTrigger_generator.cc and HitFinderCPUReceiver_generator.cc
//#
//#  JSensenig
//#  June 2019
//#  for ProtoDUNE
//# */
//###############################################################################

#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_Candidate").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/Candidate.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"
#include "dune-raw-data/Overlays/HitFragment.hh"

#include "swTrigger/AdjacencyAlgorithms.h"
#include "swTrigger/TriggerCandidate.h"

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

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

dune::SetZMQSigHandler::SetZMQSigHandler() {
  setenv("ZSYS_SIGHANDLER", "false", true);
}

// Constructor ------------------------------------------------------------------------------
dune::Candidate::Candidate(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps),
  zmq_sig_handler_(),
  instance_name_("Candidate"),
  timeout_(ps.get<int>("timeout")), 
  stopping_flag_(0),
  recvsocket_({ps.get<std::string>("recvsocket")}),
  sendsocket_({ps.get<std::string>("sendsocket")}),
  receiver_( ptmp_util::make_ptmp_socket_string("SUB","connect",recvsocket_) ),
  sender_( ptmp_util::make_ptmp_socket_string("PUB","bind",sendsocket_) ),
  tpwinsock_(ps.get<std::string>("tpwindow_input")), 
  tpwoutsock_(ps.get<std::string>("tpwindow_output")), 
  tspan_(ps.get<uint64_t>("ptmp_tspan")),
  tbuf_(ps.get<uint64_t>("ptmp_tbuffer")),
  tpsortinsock_(ps.get<std::string>("tpsorted_input")), 
  tpsortout_(ps.get<std::string>("tpsorted_output")), 
  tardy_(ps.get<int>("ptmp_tardy")),
  nTPset_recvd_(0),
  nTPset_sent_(0),
  nTPhits_(0),
  start_time_(),
  end_time_(),
  n_recvd_(0),
  norecvd_(0),
  stale_set_(0),
  loops_(0),
  fqueue_(0),
  qtpsets_(0),
  p_count_(0)
{
  DAQLogger::LogInfo(instance_name_) << "Initiated Candidate BoardReader\n";
}


// start() routine --------------------------------------------------------------------------
void dune::Candidate::start(void)
{
  stopping_flag_ = false;

  // TODO set qsize from fhicl
  size_t qsize = 100000;
  DAQLogger::LogInfo(instance_name_) << "Allocating FIFO memory, size " << qsize;

  DAQLogger::LogInfo(instance_name_) << "Setting up the PTMP sockets.";

  int felix_links = 10;
  std::vector<std::string> tpwin_;
  std::vector<std::string> tpwout_;
  std::vector<std::string> tpsortin_;

  for(int i=0; i<felix_links; i++) {
    std::string tpwin_socket = tpwinsock_ + std::to_string(141+i);
    std::string tpwout_socket = tpwoutsock_ + std::to_string(141+i);
    std::string tpsortin_socket = tpsortinsock_ + std::to_string(141+i);
    tpwin_.push_back(tpwin_socket);
    tpwout_.push_back(tpwout_socket);
    tpsortin_.push_back(tpsortin_socket);
  }

  for (auto sub : tpwin_) DAQLogger::LogInfo(instance_name_) << "TPWindow input sockets " << sub;
  for (auto pub : tpwout_) DAQLogger::LogInfo(instance_name_) << "TPWindow output sockets " << pub;
  DAQLogger::LogInfo(instance_name_) << "TPWindow Tspan " << tspan_ << " and Tbuffer " << tbuf_;

  // tspan = 2500 = 50us / 20ns and tbuffer = 150000 = 60 tspan = 3ms / 20ns
  //TPWindow connection: Felix --> TPWindow --> TPSorted
  tpwindow_01_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(0)},{tpwout_.at(0)},tspan_,tbuf_) ));
  tpwindow_02_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(1)},{tpwout_.at(1)},tspan_,tbuf_) ));
  tpwindow_03_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(2)},{tpwout_.at(2)},tspan_,tbuf_) ));
  tpwindow_04_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(3)},{tpwout_.at(3)},tspan_,tbuf_) ));
  tpwindow_05_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(4)},{tpwout_.at(4)},tspan_,tbuf_) ));
  tpwindow_06_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(5)},{tpwout_.at(5)},tspan_,tbuf_) ));
  tpwindow_07_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(6)},{tpwout_.at(6)},tspan_,tbuf_) ));
  tpwindow_08_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(7)},{tpwout_.at(7)},tspan_,tbuf_) ));
  tpwindow_09_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(8)},{tpwout_.at(8)},tspan_,tbuf_) ));
  tpwindow_10_.reset(new ptmp::TPWindow( ptmp_util::make_ptmp_tpwindow_string({tpwin_.at(9)},{tpwout_.at(9)},tspan_,tbuf_) ));

  for (auto sub : tpsortin_) DAQLogger::LogInfo(instance_name_) << "TPsorted input sockets " << sub;
  DAQLogger::LogInfo(instance_name_) << "TPsorted output socket " << tpsortout_;
  DAQLogger::LogInfo(instance_name_) << "TPsorted tardy is set to " << tardy_;

  // TPSorted connection: TPWindow --> TPSorted --> Candidate BR (default policy is drop)
  tpsorted_.reset(new ptmp::TPSorted( ptmp_util::make_ptmp_tpsorted_string(tpsortin_,{tpsortout_},tardy_) ));

  DAQLogger::LogInfo(instance_name_) << "Finished setting up TPWindow and TPsorted.";

  // Start a TPSet recieving/sending thread
  tpset_handler = std::thread(&dune::Candidate::tpsetHandler,this);
}

// tpsetHandler() routine ------------------------------------------------------------------
void dune::Candidate::tpsetHandler() {

  DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";

  while(!stopping_flag_) { 

    ptmp::data::TPSet SetReceived;

    bool received = receiver_(SetReceived, timeout_);
     
    if (!received) { ++norecvd_; continue; }

    unsigned int count = SetReceived.count();

    if (count == p_count_){
      ++stale_set_; 
      continue;
    } else {
      nTPhits_ += SetReceived.tps_size();
      ++nTPset_recvd_;
    }

    p_count_ = count;

    // TODO 
    // Pass TPSet to TC algorithm
    // Use SQCS queue to pass TCs to the getNext() loop
    // TCset_ = dune::Candidate::candFinder(SetReceived);

    // sender_(TCset_);
    sender_(SetReceived);
    ++nTPset_sent_;

    if (!queue_.isFull()) queue_.write(SetReceived);
    if (queue_.isFull()) ++fqueue_;
  }

  DAQLogger::LogInfo(instance_name_) << "Stop called, ending TPSet handler thread.";

}

// stop() routine --------------------------------------------------------------------------
void dune::Candidate::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";
  stopping_flag_ = true;

  // Not sure if this is needed not in original SWTrigger
  // also causes the BR to crash at run STOP
  //Try this, it should be more graceful
  //tpset_handler.join();

  // Should be able to call the destuctor like this
  tpwindow_01_.reset(nullptr);
  tpwindow_02_.reset(nullptr);
  tpwindow_03_.reset(nullptr);
  tpwindow_04_.reset(nullptr);
  tpwindow_05_.reset(nullptr);
  tpwindow_06_.reset(nullptr);
  tpwindow_07_.reset(nullptr);
  tpwindow_08_.reset(nullptr);
  tpwindow_09_.reset(nullptr);
  tpwindow_10_.reset(nullptr);
  tpsorted_.reset(nullptr);

  DAQLogger::LogInfo(instance_name_) << "Destroyed PTMP windowing and sorting threads.";

  // Write to log some end of run stats here
  DAQLogger::LogInfo(instance_name_) << "Received " << nTPhits_ << " hits in " << nTPset_recvd_ << " TPsets with " 
                                     << stale_set_ << " stale TPSets and " << norecvd_ << " TPsets not received";
  DAQLogger::LogInfo(instance_name_) << "Received " << nTPset_recvd_ << " TSets and sent " << nTPset_sent_ << " TPsets";
  DAQLogger::LogInfo(instance_name_) << "Elapsed time receiving TPsets (s) " << std::chrono::duration_cast<std::chrono::duration<double>>(end_time_ - start_time_).count();
  DAQLogger::LogInfo(instance_name_) << "Number of non-nullptr " << qtpsets_ << " in " << loops_ << " getNext() loops and number of full queue loops " << fqueue_; 

}


// No mutex stoproutine -----------------------------------------------------------------------
void dune::Candidate::stopNoMutex(void)
{
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";
}


// Check HW Status() routine ------------------------------------------------------------------
bool dune::Candidate::checkHWStatus_()
{
  // No asssociated hardware to check so don't worry, just return true.
  return true;
}


// getNext() routine --------------------------------------------------------------------------
bool dune::Candidate::getNext_(artdaq::FragmentPtrs&)
{

  start_time_ = std::chrono::high_resolution_clock::now(); 

  // Catch the stop flag at beginning of getNext loop
  if (stopping_flag_) return false;

  tpset_ = queue_.frontPtr();
  ++loops_;
  if (tpset_) {
    ++qtpsets_;
    //DAQLogger::LogInfo(instance_name_) << "Received TPset count " << tpset_->count() << "  " << tpset_; 
    queue_.popFront();
  }


  // If it made it here, assume the prescale has been met so,
  // 1. Send the TPSet onward
  // 2. Fill and buffer an artDAQ fragment TODO

  // Not quite ready for this! Need to finalize an artDAQ "Candidate" fragment type for the 
  /*
  // Fill the artDAQ fragment
  std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( TimingFragment::size()*sizeof(uint32_t),
                                                                            artdaq::Fragment::InvalidSequenceID,
                                                                            artdaq::Fragment::InvalidFragmentID,
                                                                            artdaq::Fragment::InvalidFragmentType,
                                                                            dune::detail::CPUHITS,
                                                                            dune::HitFragment::Metadata(HitFragment::VERSION));
  // It's unclear to me whether the constructor above actually sets
  // the metadata, so let's do it here too to be sure
  f->updateMetadata(HitFragment::Metadata(HitFragment::VERSION));
      
  // Fill in the fragment header fields (not some other fragment
  // generators may put these in the
  // constructor for the fragment, but here we push them in one at a time.
  f->setSequenceID(ev_counter());
  f->setFragmentID(fragment_id()); // fragment_id is in our base class, fhicl sets it
  f->setUserType(dune::detail::CPUHITS);

  dune::HitFragment hitfrag(*f);
  f->setTimestamp(HitSet.tps()[it].tstart()); // TS not going to work FIXME   

  // not too sure why we need this one again here...
    
  frags.emplace_back(std::move(f));
  */
  
  // We only increment the event counter for events we send out
  ev_counter_inc();

  end_time_ = std::chrono::high_resolution_clock::now(); 

  return true;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::Candidate)
