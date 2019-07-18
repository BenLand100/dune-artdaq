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
  prev_count_(0)
{
  DAQLogger::LogInfo(instance_name_) << "Initiated Candidate BoardReader\n";
  if(tpwinsocks_.size()!=tpwoutsocks_.size()){
      throw cet::exception("Size of TPWindow input and output socket configs do not match. Check tpwindow_inputs and tpwindow_outputs fhicl parameters");
  }
}

dune::Candidate::~Candidate()
{
    DAQLogger::LogInfo(instance_name_) << "dtor";
}

// start() routine --------------------------------------------------------------------------
void dune::Candidate::start(void)
{
  stopping_flag_ = false;

  // TODO set qsize from fhicl
  size_t qsize = 100000;
  DAQLogger::LogInfo(instance_name_) << "Allocating FIFO memory, size " << qsize;

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

  // FIXME replace with TPZipper. Equivalent except replace parameter "tardy" --> "sync_time"
  // TPSorted connection: TPWindow --> TPSorted --> Candidate BR (default policy is drop)
  tpzipper_.reset(new ptmp::TPZipper( ptmp_util::make_ptmp_tpsorted_string(tpzipinsocks_,{tpzipout_},tardy_) ));

  DAQLogger::LogInfo(instance_name_) << "Finished setting up TPWindow and TPsorted.";

  // Start a TPSet recieving/sending thread
  tpset_handler = std::thread(&dune::Candidate::tpsetHandler,this);
}

// tpsetHandler() routine ------------------------------------------------------------------
void dune::Candidate::tpsetHandler() {

  pthread_setname_np(pthread_self(), "tpsethandler");

  DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";

  ptmp::TPReceiver receiver( ptmp_util::make_ptmp_socket_string("SUB","connect",{recvsocket_}) );
  ptmp::TPSender sender( ptmp_util::make_ptmp_socket_string("PUB","bind",{sendsocket_}) );

  std::vector<ptmp::data::TPSet> aggrSets;
  int max_adj = 0;
  int min_adj = 0;
  unsigned int handlerloop = 0;
  unsigned int aggrloop = 0;
  unsigned int aggr_size = 0;
  unsigned int tc_sorted_size = 0;
  unsigned int tc_count = 0;
  unsigned int avg_adj = 0;
  uint64_t prev_tstart = 0;
  uint64_t timetot = 0;
  // TODO fhicl param to send TPs or TCs
  bool sendTC = true;
  auto start_run = std::chrono::high_resolution_clock::now();

  while(!stopping_flag_) { 
    
    bool aggregate = true;
    
    while(aggregate && !stopping_flag_) { 
      
      ptmp::data::TPSet SetReceived;
      bool received = receiver(SetReceived, timeout_);
      
      if (!received) { ++norecvd_; continue; }
    
      nTPhits_ += SetReceived.tps_size();

      aggrSets.push_back(SetReceived);

      // Aggregate TPSets from same time window
      if (prev_tstart != SetReceived.tstart() || !sendTC) aggregate = false;
      prev_tstart = SetReceived.tstart();
      ++aggrloop;
    } 

    if (!sendTC) {
      sender(aggrSets.at(0)); 
      ++nTPset_sent_;
      if (!queue_.isFull()) queue_.write(aggrSets.at(0)); else ++fqueue_;
    } else {
      // Channel sort and map ptmp::TrigPrim to TP
      auto start_sort = std::chrono::high_resolution_clock::now();
      std::vector<TP> sortedTPs = TPChannelSort(aggrSets);
      
      // Pass the TPs to the Candidate Alg (2nd arg:  0=adjacency, 1=clustering)
      std::vector<int> trigcands = TriggerCandidate(sortedTPs, 0);

      if (!(trigcands.size() > 0)) continue;

      if (trigcands.size() > 0) { 
        ++tc_count; tc_sorted_size += sortedTPs.size(); avg_adj += trigcands[0];
        max_adj = std::max(max_adj, trigcands[0]); min_adj = std::min(max_adj, trigcands[0]);
        // Map TC to TPSet
        ptmp::data::TPSet SetToSend;
        SetToSend.set_count(tc_count);
        SetToSend.set_chanend(trigcands[4]);
        SetToSend.set_chanbeg(trigcands[5]);
        SetToSend.set_tstart(trigcands[6]);
        SetToSend.set_tspan(trigcands[7] - trigcands[6]);
        // APA5=0 APA6=1 APA4=2 TODO make this number fhicl param.
        SetToSend.set_detid(0);
        uint64_t tpsetcreate = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        SetToSend.set_created(tpsetcreate);
        for (size_t it=0; it<sortedTPs.size(); ++it) { 
          ptmp::data::TrigPrim* ptmp_prim=SetToSend.add_tps();
          // Note: sortedTPs is vector of TP struct NOT ptmp::TrigPrim
          ptmp_prim->set_channel(sortedTPs[it].channel);
          ptmp_prim->set_tstart (sortedTPs[it].tstart );
          ptmp_prim->set_tspan  (sortedTPs[it].tspan  );
          ptmp_prim->set_adcsum (sortedTPs[it].adcsum );
        }
        sender(SetToSend);
        auto sent_time = std::chrono::high_resolution_clock::now();
        timetot = std::chrono::duration_cast< std::chrono::microseconds >( sent_time - start_sort ).count();
      }
    }
    ++handlerloop;
    aggr_size += aggrSets.size();
    aggrSets.clear();
  }
  auto end_run = std::chrono::high_resolution_clock::now();
  uint64_t runtime = std::chrono::duration_cast< std::chrono::microseconds >( end_run - start_run ).count();
    
  DAQLogger::LogInfo(instance_name_) << "Stop called, ending TPSet handler thread.";
  if (tc_count != 0) {
	  DAQLogger::LogInfo(instance_name_) << "Avg. number of TPsets aggregated " << (aggr_size / handlerloop);
	  DAQLogger::LogInfo(instance_name_) << "Avg. number of TPs per triggered sorted vector " << (tc_sorted_size / tc_count);
	  DAQLogger::LogInfo(instance_name_) << "Avg. time from start sort to TC sent " << (timetot / tc_count);
	  DAQLogger::LogInfo(instance_name_) << "Generated " << tc_count << " TCs with avg adjacency " << (avg_adj / tc_count)
					     << " with run time " << runtime << "us with avg TC rate " << (tc_count / runtime)*1e6 << "Hz";
	  DAQLogger::LogInfo(instance_name_) << "Adjacency: min " << min_adj << " max " << max_adj << " avg " << (avg_adj / tc_count);
  } else {
	  DAQLogger::LogInfo(instance_name_) << "TC count is 0";
  }
}

// TPChannelSort() routine --------------------------------------------------------------------------
/** 
 *  
 * The sorting function assumes, 
 *  - input:  aggregated vector of time-ordered TPSets (TPZipper takes care of time ordering TPSets)
 *  - output: vector<TP> channel-sorted and time sub-ordered for a single time window with 
 *            ptmp::TrigPrim mapped to TP struct
 * Note: see AdjacencyAlgorithms.h for definition of TP struct
 *
 **/

std::vector<TP>
dune::Candidate::TPChannelSort(std::vector<ptmp::data::TPSet> HitSets) {
  
  // - All the aggregated TPsets are disassembled into their TPs
  // - These are sorted such that ALL the aggregated TPs are in one vector
  // - This single vector is passed as the input to the adjacency alg
  // - This assumes sorting within a single time window

  std::vector<TP> new_TPs;
  TP trigprim;

  for (auto const& HitSet : HitSets) { //loop over aggregated TPsets
    for (int it=0; it<HitSet.tps_size(); ++it) { //Loop over TPs
      //If we have to loop anyway.. map ptmp::TrigPrim to Candidate TP struct
      trigprim.channel = HitSet.tps()[it].channel();
      trigprim.tstart  = HitSet.tps()[it].tstart();
      trigprim.tspan   = HitSet.tps()[it].tspan();
      trigprim.adcsum  = HitSet.tps()[it].adcsum();
      trigprim.adcpeak = HitSet.tps()[it].adcpeak();
      trigprim.flags   = HitSet.tps()[it].flags();

      if (new_TPs.size() == 0){  
        new_TPs.push_back(trigprim);
      } else if (trigprim.channel > new_TPs.back().channel) {
        new_TPs.push_back(trigprim);
      } else if (trigprim.channel == new_TPs.back().channel) { 
        if (trigprim.tstart >=  new_TPs.back().tstart) { 
          new_TPs.push_back(trigprim);
        } else { //sub-order in time
          for (unsigned int j=1; j < new_TPs.size(); ++j) {
            if (trigprim.tstart > new_TPs[new_TPs.size()-(j+1)].tstart){
              new_TPs.insert(new_TPs.end()-j,trigprim);
              break;
            }
          }
        }
      } else { //otherwise loop through vec and find TP place wrt channel
        for (unsigned int j=0; j < new_TPs.size(); ++j) {
        if (trigprim.channel == new_TPs.back().channel) { 
          if (trigprim.tstart >=  new_TPs.back().tstart) {
            new_TPs.push_back(trigprim);
          } else { //sub-order in time
            for (unsigned int j=1; j < new_TPs.size(); ++j) {
              if (trigprim.tstart > new_TPs[new_TPs.size()-(j+1)].tstart){
                new_TPs.insert(new_TPs.end()-j,trigprim);
                break;
              }
            }
          }
        } else if (trigprim.channel < new_TPs[j].channel) {
            new_TPs.insert(new_TPs.begin()+j,trigprim);
            break;
          }
        }
      }
    }
  }
  return new_TPs;
}

// stop() routine --------------------------------------------------------------------------
void dune::Candidate::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";
  stopping_flag_ = true;

  DAQLogger::LogInfo(instance_name_) << "Joining tpset_handler thread...";
  tpset_handler.join();
  DAQLogger::LogInfo(instance_name_) << "tpset_handler thread joined";

  // Destruct all the TPWindow instances and the TPZipper
  tpzipper_.reset(nullptr);
  tpwindows_.clear(); 


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
