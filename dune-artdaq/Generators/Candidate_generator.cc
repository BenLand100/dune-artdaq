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

#include "artdaq/Generators/GeneratorMacros.hh"
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

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

// Constructor ------------------------------------------------------------------------------
dune::Candidate::Candidate(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps),
  instance_name_("Candidate"),
  timeout_(ps.get<int>("timeout")), 
  stopping_flag_(0),
  tpwinsocks_(ptmp_util::endpoints_for_key(ps, "tpwindow_input_connections_key")), 
  tpwoutsocks_(ptmp_util::endpoints_for_key(ps, "tpwindow_output_connections_key")), 
  tspan_(ps.get<uint64_t>("ptmp_tspan")),
  tbuf_(ps.get<uint64_t>("ptmp_tbuffer")),
  tpzipinsocks_(tpwoutsocks_), 
  tpzipout_(ps.get<std::string>("tpsorted_output")), 
  tardy_(ps.get<int>("ptmp_tardy")),
  recvsocket_(tpzipout_),
  sendsocket_({ps.get<std::string>("tc_output")}),
  tc_alg_(ps.get<std::string>("TC_algorithm")),
  nTPset_recvd_(0),
  start_time_(),
  end_time_(),
  n_recvd_(0),
  loops_(0)
{
  DAQLogger::LogInfo(instance_name_) << "Initiated Candidate BoardReader\n";
  if(tpwinsocks_.size()!=tpwoutsocks_.size()){
      throw cet::exception("Size of TPWindow input and output socket configs do not match. Check tpwindow_inputs and tpwindow_outputs fhicl parameters");
  }
  std::vector<std::string> libs=ps.get<std::vector<std::string>>("ptmp_plugin_libraries");
  bool success=ptmp_util::add_plugin_libraries(libs);
  if(!success){
      std::ostringstream ss;
      ss << "Couldn't load one of the requested ptmp_plugin_libraries: ";
      for(auto const& lib: libs) ss << lib << ", ";
      DAQLogger::LogError(instance_name_) << ss.str();
  }
  
}

dune::Candidate::~Candidate()
{
    DAQLogger::LogInfo(instance_name_) << "dtor";
}

// start() routine --------------------------------------------------------------------------
void dune::Candidate::start(void)
{
  stopping_flag_.store(false);

  DAQLogger::LogInfo(instance_name_) << "Setting up the PTMP sockets.";
  DAQLogger::LogInfo(instance_name_) << "TPWindow Tspan " << tspan_ << " and Tbuffer " << tbuf_;

  if(tpwinsocks_.size()!=tpwoutsocks_.size()){
      DAQLogger::LogError(instance_name_) << "TPWindow input and output lists are different lengths!";
  }
  for(size_t i=0; i<tpwinsocks_.size(); ++i){
      DAQLogger::LogInfo(instance_name_) << "Creating TPWindow from " << tpwinsocks_.at(i) << " to " << tpwoutsocks_.at(i);
      std::string jsonconfig{ptmp_util::make_ptmp_tpwindow_string({tpwinsocks_.at(i)},{tpwoutsocks_.at(i)},tspan_,tbuf_)};
      tpwindows_.push_back( std::make_unique<ptmp::TPWindow>(jsonconfig) );
  }

  DAQLogger::LogInfo(instance_name_) << "TPZipper tardy is set to " << tardy_;

  // TPZipper connection: TPWindow --> TPZipper --> TPFilter (default policy is drop)
  tpzipper_.reset(new ptmp::TPZipper( ptmp_util::make_ptmp_tpsorted_string(tpzipinsocks_,{tpzipout_},tardy_) ));

  DAQLogger::LogInfo(instance_name_) << "Finished setting up TPWindow and TPZipper.";
  DAQLogger::LogInfo(instance_name_) << "Starting Candidate algorithm: " << tc_alg_;

  //                                         --> to MLT
  // TPWindow --> TPZipper --> TPFilter --> |
  //                                         --> to getNext()
  tcGen_.reset(new ptmp::TPFilter( ptmp_util::make_ptmp_tpfilter_string({recvsocket_}, {sendsocket_}, tc_alg_, "tpfilter") ));

  start_time_ = std::chrono::high_resolution_clock::now(); 
  DAQLogger::LogInfo(instance_name_) << "Finished starting Candidate algorithm thread.";

}


// stop() routine --------------------------------------------------------------------------
void dune::Candidate::stop(void)
{
  end_time_ = std::chrono::high_resolution_clock::now(); 
  DAQLogger::LogInfo(instance_name_) << "stop() called";
  stopping_flag_.store(true);

  // Destruct all the TPWindow instances, TPZipper, and TPFilter
  tcGen_.reset(nullptr);
  tpzipper_.reset(nullptr);
  tpwindows_.clear(); 

  DAQLogger::LogInfo(instance_name_) << "Destroyed PTMP windowing and sorting threads.";

  // Write to log some end of run stats here
  DAQLogger::LogInfo(instance_name_) << "Received " << nTPset_recvd_ << " TPsets"; 
  DAQLogger::LogInfo(instance_name_) << "getNext() loops " << loops_;
  DAQLogger::LogInfo(instance_name_) << "Runtime from start() to stop() (sec) " << std::chrono::duration_cast<std::chrono::duration<double>>(end_time_ - start_time_).count();

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


  // Catch the stop flag at beginning of getNext loop
  if (stopping_flag_.load()) return false;

  ++loops_;


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


  // Sleep a bit so we don't spin the CPU
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return true;

}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::Candidate)
