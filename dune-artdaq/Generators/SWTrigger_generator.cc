//################################################################################
//# /*
//#        SWTrigger_generator.cpp
//#
//#  GLM Mar 2019: dummy version receiving TS infor from timing and generating a trigger on it.
//# */
//################################################################################

#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_SWTrigger").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/SWTrigger.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"
#include "dune-artdaq/Generators/swTrigger/trigger_util.hh"

#include "artdaq/Generators/GeneratorMacros.hh"
#include "cetlib/exception.h"
// #include "dune-raw-data/Overlays/FragmentType.hh"
// #include "dune-raw-data/Overlays/TimingFragment.hh"
// #include "dune-raw-data/Overlays/TriggerFragment.hh"

#include "fhiclcpp/ParameterSet.h"
#include "timingBoard/InhibitGet.h" // The interface to the ZeroMQ trigger inhibit master

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <set>
#include <random>

#include <unistd.h>

#include <cstdio>

#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push
#pragma GCC diagnostic pop

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;

dune::SetZMQSigHandler::SetZMQSigHandler() {
  setenv("ZSYS_SIGHANDLER", "false", true);
}

// Constructor ------------------------------------------------------------------------------
dune::SWTrigger::SWTrigger(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps)
  ,zmq_sig_handler_()
  ,instance_name_("SWTrigger")
  ,stopping_flag_(0)
  ,throttling_state_(true)
  ,inhibitget_timer_(ps.get<uint32_t>("inhibit_get_timer",5000000)) 
  ,partition_number_(ps.get<uint32_t>("partition_number",0))
  ,debugprint_(ps.get<uint32_t>("debug_print",0))
  ,zmq_conn_(ps.get<std::string>("zmq_connection","tcp://pddaq-gen05-daq0:5566"))
  ,want_inhibit_(false)
  ,last_spillstart_tstampl_(0xffffffff) // Timestamp of most recent start-of-spill
  ,last_spillstart_tstamph_(0xffffffff) // ...
  ,last_spillend_tstampl_(0xffffffff)   // Timestamp of most recent end-of-spill
  ,last_spillend_tstamph_(0xffffffff)   // ...
  ,last_runstart_tstampl_(0xffffffff)   // Timestamp of most recent start-of-run
  ,last_runstart_tstamph_(0xffffffff)   // ...
  ,sender_( ptmp_util::make_ptmp_socket_string("PUB","bind",{"tcp://*:50502"}) )
  ,timeout_(ps.get<int>("timeout"))
  ,n_recvd_(0)
  ,n_inputs_(ptmp_util::endpoints_for_key(ps, "tc_inputs_key").size())
  ,prev_counts_(std::vector<size_t>(n_inputs_, 0))
  ,norecvds_(std::vector<size_t>(n_inputs_, 0))
  ,n_recvds_(std::vector<size_t>(n_inputs_, 0))
  ,nTPhits_(std::vector<size_t>(n_inputs_, 0))
  ,ntriggers_(0)
  ,fqueue_(0)
  ,loops_(0)
  ,qtpsets_(0)
  ,count_(0)
  ,trigger_holdoff_time_(ps.get<uint64_t>("trigger_holdoff_time_pdts_ticks", 6000*25))
{

  std::stringstream instance_name_ss;
  instance_name_ss << instance_name_ << partition_number_;
  instance_name_ = instance_name_ss.str();

  if (inhibitget_timer_ == 0) inhibitget_timer_ = 2000000000;  // Zero inhibitget_timer waits infinite.

  // Set up connection to Inhibit Master. This is the inbound
  // connection (ie, InhibitMaster talks to us to say whether we
  // should enable triggers). We'll actually call
  // InhibitGet_connect() in start(), in order to do so as late as
  // possible and minimize the chance of reading stale messages from
  // the InhibitMaster
  InhibitGet_init(inhibitget_timer_);

  std::vector<std::string> tc_inputs=ptmp_util::endpoints_for_key(ps, "tc_inputs_key");
  for(auto const& tc_input: tc_inputs){
    std::string sock_str(ptmp_util::make_ptmp_socket_string("SUB","connect",{tc_input}));
    receivers_.push_back(std::make_unique<ptmp::TPReceiver>(sock_str));
  }
  // Set up outgoing connection to InhibitMaster: this is where we
  // broadcast whether we're happy to take triggers
  status_publisher_.reset(new artdaq::StatusPublisher(instance_name_, ps.get<std::string>("zmq_connection_out","tcp://*:5599")));
  status_publisher_->BindPublisher();

  fragment_publisher_.reset(new artdaq::FragmentPublisher(ps.get<std::string>("zmq_fragment_connection_out","tcp://*:7123")));
  fragment_publisher_->BindPublisher();

  ts_receiver_.reset(new dune::HwClockSubscriber(ps.get<std::string>("zmq_connection_ts","tcp://*:5566")));
  DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";
}

// start() routine --------------------------------------------------------------------------
void dune::SWTrigger::start(void)
{
  DAQLogger::LogDebug(instance_name_) << "start() called\n";

  // See header file for meanings of these variables
  stopping_flag_.store(false);
  throttling_state_ = true;    // 0 Causes it to start triggers immediately, 1 means wait for InhibitMaster to release
  previous_ts_ = 0;
  latest_ts_ = 0;
  int isConnected = ts_receiver_->connect(100);    

  if (isConnected!=0){
    dune::DAQLogger::LogWarning(instance_name_) << "ZMQ connect for TS return code is not zero, but: " << isConnected;
  }
  else{
    dune::DAQLogger::LogInfo(instance_name_) << "Connected successfully to ZMQ TS Socket!";
  }

  InhibitGet_connect(zmq_conn_.c_str());
  InhibitGet_retime(inhibitget_timer_);

  ts_subscriber_.reset(new std::thread(&dune::SWTrigger::readTS, this));

  // Start a TPSet recieving/sending thread
  tpset_handler = std::thread(&dune::SWTrigger::tpsetHandler, this);

}

void dune::SWTrigger::readTS() {
  while (!stopping_flag_.load()) {
    latest_ts_ = ts_receiver_->receiveHardwareTimeStamp();
  }
}

// tpsetHandler() routine ------------------------------------------------------------------

void dune::SWTrigger::tpsetHandler() {

  pthread_setname_np(pthread_self(), "tpsethandler");

  DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";

  std::vector<bool> received(n_inputs_, false);

  while(!stopping_flag_.load()) {

    // TODO: the inputs should probably go through TPSorted/TPZipper instead
    std::vector<ptmp::data::TPSet> SetsReceived(n_inputs_);

    // Attempt to receive a trigger candidate on each input
    for(size_t i=0; i<n_inputs_; ++i){
      received[i] = (*receivers_[i])(SetsReceived[i], timeout_);
    }

    for(size_t i=0; i<receivers_.size(); ++i){
      if(received[i])  ++norecvds_[i];
      else             ++n_recvds_[i];

      nTPhits_[i]+=SetsReceived[i].tps_size();

      if(prev_counts_[i]!=0 && (SetsReceived[i].count()!=prev_counts_[i]+1)){
        // Somehow signal that we missed an item on this input
      }
      prev_counts_[i]=SetsReceived[i].count();
    }

    // If we didn't get a set on every link, don't do any more this round
    if(!std::all_of(received.begin(), received.end(), [](bool p) {return p;})){
      continue;
    }

    ++n_recvd_;

    if (n_recvd_ < 4400) continue;

    n_recvd_ = 0;
    ++ntriggers_;

    // Add the TPsets to the queue to allow access from getNext
    bool write_success=true;
    for(auto const& set: SetsReceived){
      write_success = write_success && timestamp_queue_.write(set.tstart());
    }

    if(!write_success) ++fqueue_;
    
    
  }

  DAQLogger::LogInfo(instance_name_) << "Stop called, ending TPSet handler thread.";

}

void dune::SWTrigger::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";

  stopping_flag_.store(true);   // We do want this here, if we don't use an
  // atomic<int> for stopping_flag_ (see header file comments)

  // Make sure the TPReceiver dtors get called
  for(auto & recv: receivers_) recv.reset();

  DAQLogger::LogInfo(instance_name_) << "Joining threads.";
  tpset_handler.join();
  DAQLogger::LogInfo(instance_name_) << "Threads joined.";

  std::ostringstream ss_stats;
  ss_stats << "Statistics by input link:" << std::endl;

  ss_stats << "Number of TP(TC)Sets not received ";
  for(auto const& norecvd: norecvds_) ss_stats << std::setw(7) << norecvd;
  ss_stats << std::endl;

  ss_stats << "Number of TP(TC)Sets received     ";
  for(auto const& n_recvd: n_recvds_) ss_stats << std::setw(7) << n_recvd;
  ss_stats << std::endl;

  ss_stats << "Number of TrigPrims received      ";
  for(auto const& nTPhit: nTPhits_) ss_stats << std::setw(7) << nTPhit;
  ss_stats << std::endl;

  ss_stats << "Final TP(TC)Set count()           ";
  for(auto const& prev_count: prev_counts_) ss_stats << std::setw(7) << prev_count;
  ss_stats << std::endl;

  ss_stats << std::endl;

  ss_stats << "Number of triggers " << ntriggers_ << std::endl;
  ss_stats << "Number of TPsets received in getNext " << qtpsets_ << " in loops " << loops_ << std::endl;
  ss_stats << "Number of triggers " << ntriggers_ << std::endl;

  DAQLogger::LogInfo(instance_name_) << ss_stats.str();

}

void dune::SWTrigger::stopNoMutex(void)
{
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";

  // (PAR 2018-10-15) Change this to *not* set stopping_flag_: that
  // variable is a bare int (not std::atomic) and stopnoMutex() is
  // called from a different thread from getNext_(). So this means
  // that stopping_flag_ can change in the middle of a getNext()
  // call, which would break the logic of getNext_() and may be
  // causing the "getNext: std::exception caught: Failed to stop
  // after 5000 milliseconds" errors.
  //
  // This flag is instead set in stop(), which is guaranteed to not
  // be called while getNext_() is running

  // stopping_flag_ = 1;    // Tells the getNext_() while loop to stop the run
}


bool dune::SWTrigger::checkHWStatus_()
{
  auto dsmptr = artdaq::BoardReaderCore::GetDataSenderManagerPtr();
  if(!dsmptr)
    TLOG(TLVL_HWSTATUS) << "DataSenderManagerPtr not valid.";

  int n_remaining_table_entries = dsmptr? dsmptr->GetRemainingRoutingTableEntries() : -1;
  int n_table_count = dsmptr? dsmptr->GetRoutingTableEntryCount() : -1;
  TLOG(TLVL_HWSTATUS)	<< " table_count=" << n_table_count
                        << " table_entries_remaining=" << n_remaining_table_entries;

  bool new_want_inhibit=false;
  std::string status_msg="";

  //check if there are available routing tokens, and if not inhibit
  if(n_remaining_table_entries==0){
    new_want_inhibit=true;
    status_msg="NoAvailableTokens";
  }
  else{
    new_want_inhibit=false;
  }

  if(new_want_inhibit && !want_inhibit_){
    DAQLogger::LogInfo(instance_name_) << "Want inhibit Status change: Publishing bad status: \"" << status_msg << "\"";
    status_publisher_->PublishBadStatus(status_msg);
  }
  if(want_inhibit_ && !new_want_inhibit){
    DAQLogger::LogInfo(instance_name_) << "Want inhibit status change: Publishing good status";
    status_publisher_->PublishGoodStatus();
  }

  want_inhibit_ = new_want_inhibit;

  return true;
}


#define D(a)
#define E(a) a

bool dune::SWTrigger::getNext_(artdaq::FragmentPtrs &frags)
{

  static uint64_t prev_timestamp=0;

  // TODO: Change the check here to "if stopping_flag && we've got all of the fragments in the queue"
  if (stopping_flag_.load()) return false;

  uint32_t tf = InhibitGet_get();  
  if (tf==1) {
    throttling_state_=false;
  }
  else if (tf ==2) {
    throttling_state_= true;
  }

  uint64_t* trigger_timestamp = timestamp_queue_.frontPtr();
  if (trigger_timestamp) {
    DAQLogger::LogInfo(instance_name_) << "Trigger requested for 0x" << std::hex << (*trigger_timestamp) << std::dec;
    if(*trigger_timestamp < prev_timestamp+trigger_holdoff_time_){
      DAQLogger::LogInfo(instance_name_) << "Trigger too close to previous trigger time of " << prev_timestamp << ". Not sending";
    }
    else{
      // Only send a fragment if the inhibit master hasn't inhibited us
      if(!throttling_state_){
        std::unique_ptr<artdaq::Fragment> frag=trigger_util::makeTriggeringFragment(*trigger_timestamp, ev_counter(), fragment_id());
        dune::TimingFragment timingFrag(*frag);    // Overlay class
        
        int pubSuccess = fragment_publisher_->PublishFragment(frag.get(), &timingFrag);
        if (!pubSuccess)
          DAQLogger::LogInfo(instance_name_) << "Publishing fragment to ZeroMQ failed";
        
        frags.emplace_back(std::move(frag));
        // We only increment the event counter for events we send out
        ev_counter_inc();
            
        prev_timestamp=*trigger_timestamp;
      }
    }
    // We always pop the item off the queue, even if we didn't send it
    // out, otherwise the queue will just back up
    timestamp_queue_.popFront();
  }
  // Sleep a bit so we don't spin the CPU
  std::this_thread::sleep_for(std::chrono::milliseconds(1));

  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::SWTrigger)
