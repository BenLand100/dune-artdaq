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

#include "artdaq/Application/BoardReaderCore.hh"
#include "artdaq/Generators/GeneratorMacros.hh"


#include "dune-artdaq/Generators/SWTrigger.hh"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"
#include "dune-artdaq/Generators/swTrigger/trigger_util.hh"
#include "dune-artdaq/Generators/swTrigger/fhicl-to-json.hh"

#include "fhiclcpp/ParameterSet.h"
#include "timingBoard/InhibitGet.h" // The interface to the ZeroMQ trigger inhibit master

#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <random>

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
  ,zmq_conn_(ps.get<std::string>("zmq_connection","tcp://pddaq-gen05-daq0:5566"))
  ,want_inhibit_(false)
  ,sender_( ptmp_util::make_ptmp_socket_string("PUB","bind",{"tcp://*:50502"}) )
  ,timeout_(ps.get<int>("timeout"))
  ,n_recvd_(0)
  ,n_inputs_(ptmp_util::endpoints_for_key(ps, "tc_inputs_key").size())
  ,faketrigger_(ps.get<bool>("fake_trigger"))
  ,prescale_(ps.get<int>("faketrigger_prescale"))
  ,tczipout_(ps.get<std::string>("TCZipper_output"))
  ,tardy_(ps.get<int>("tardy"))
  ,tdout_(ps.get<std::string>("TD_output"))
  ,td_alg_(ps.get<std::string>("TD_algorithm"))
  ,td_alg_config_json_(fhicl_to_json::jsonify(ps.get<fhicl::ParameterSet>("TD_algorithm_config")))
  ,prev_counts_(0)
  ,norecvds_(0)
  ,n_recvds_(0)
  ,nTPhits_(0)
  ,ntriggers_(0)
  ,fqueue_(0)
  ,loops_(0)
  ,qtpsets_(0)
  ,count_(0)
  ,trigger_holdoff_time_(ps.get<uint64_t>("trigger_holdoff_time_pdts_ticks", 25000000))
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

  tc_inputs_ = ptmp_util::endpoints_for_key(ps, "tc_inputs_key");
 
  // Set up outgoing connection to InhibitMaster: this is where we
  // broadcast whether we're happy to take triggers
  status_publisher_.reset(new artdaq::StatusPublisher(instance_name_, ps.get<std::string>("zmq_connection_out","tcp://*:5599")));
  status_publisher_->BindPublisher();

  fragment_publisher_.reset(new artdaq::FragmentPublisher(ps.get<std::string>("zmq_fragment_connection_out","tcp://*:7123")));
  fragment_publisher_->BindPublisher();

  ts_receiver_.reset(new dune::HwClockSubscriber(ps.get<std::string>("zmq_connection_ts","tcp://*:5566")));

  dune::DAQLogger::LogInfo("SWTrigger::metrics_thread") << "Creating metrics thread";
  metricsThread = std::thread(&SWTrigger::metrics_thread, this);
  // TODO: This code is duplicated in the candidate generator
  DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";

  std::vector<std::string> libs=ps.get<std::vector<std::string>>("ptmp_plugin_libraries");
  bool success=ptmp_util::add_plugin_libraries(libs);
  if(!success){
      std::ostringstream ss;
      ss << "Couldn't load one of the requested ptmp_plugin_libraries: ";
      for(auto const& lib: libs) ss << lib << ", ";
      DAQLogger::LogError(instance_name_) << ss.str();
  }

}

// start() routine --------------------------------------------------------------------------
void dune::SWTrigger::start(void)
{
  DAQLogger::LogDebug(instance_name_) << "start() called\n";

  // See header file for meanings of these variables
  stopping_flag_.store(false);
  throttling_state_ = true;    // 0 Causes it to start triggers immediately, 1 means wait for InhibitMaster to release
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

  // TPZipper connection: TPZipper --> TPFilter (default policy is drop)
  tczipper_.reset(new ptmp::TPZipper( ptmp_util::make_ptmp_tpsorted_string(tc_inputs_,{tczipout_},tardy_) ));

  // TPFilter to run module algorithm
  nlohmann::json algconfig=nlohmann::json::parse(td_alg_config_json_);
  if(!faketrigger_) tdGen_.reset(new ptmp::TPFilter( ptmp_util::make_ptmp_tpfilter_string({tczipout_},
                                                                                          {tdout_},
                                                                                          td_alg_,
                                                                                          "td_gen",
                                                                                          &algconfig) ));
  DAQLogger::LogInfo(instance_name_) << "Started TPZipper and TPFilter with algorithm " << td_alg_;

  // Start a TPSet recieving/sending thread
  tpset_handler = std::thread(&dune::SWTrigger::tpsetHandler, this);

}

void dune::SWTrigger::readTS() {
  while (!stopping_flag_.load()) {
    latest_ts_ = ts_receiver_->receiveHardwareTimeStamp();
  }
}

// tpsetHandler() routine ------------------------------------------------------------------


void dune::SWTrigger::metrics_thread() {
//  
  unsigned int metric_reporting_interval_seconds = 10;
  ptmp::data::TPSet SetReceived_for_metrics;
  ptmp::TPReceiver* receiver_for_metrics = new ptmp::TPReceiver( ptmp_util::make_ptmp_socket_string("SUB","connect",{tczipout_}) ); //Connect to TPZipper
  long unsigned int TPSet_count = 0;
  long unsigned int last_nTPhits = 0;
  auto start = std::chrono::system_clock::now();

  while(!stopping_flag_.load()){
    if (artdaq::Globals::metricMan_) {
      TPSet_count = nTPhits_ - last_nTPhits;
      last_nTPhits = nTPhits_;
      auto end = std::chrono::system_clock::now();
      auto elapsed_to_cast = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
      long unsigned int elapsed = elapsed_to_cast.count();
      dune::DAQLogger::LogInfo("SWTrigger::metrics_thread") << "Publishing metrics TPSets: " << TPSet_count << " time: " << elapsed;
      artdaq::Globals::metricMan_->sendMetric("TPSets",  TPSet_count,  "sets", 1, artdaq::MetricMode::LastPoint);
      artdaq::Globals::metricMan_->sendMetric("Time",  elapsed,  "ms", 1, artdaq::MetricMode::LastPoint);
      TPSet_count = 0;
      start = std::chrono::system_clock::now();
    }
    for(size_t i=0; i<metric_reporting_interval_seconds; ++i){
      std::this_thread::sleep_for(std::chrono::seconds(1));
      if(stopping_flag_.load()) break;
    }
  }
  delete receiver_for_metrics;
}

void dune::SWTrigger::tpsetHandler() {

  pthread_setname_np(pthread_self(), "tpsethandler");
  DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";
  
  std::vector<std::string> recvr_sock;

  if(!faketrigger_) recvr_sock = {tdout_}; //{"tcp://BR_IP:50001"};
  else recvr_sock = {tczipout_}; //{"tcp://BR_IP:45001"};
  if(!faketrigger_) DAQLogger::LogInfo(instance_name_) << "Connecting to TPFilter (TDs) at " << tdout_;
  else DAQLogger::LogInfo(instance_name_) << "Connecting to TPZipper (TCs) at " << tczipout_;

  ptmp::TPReceiver* receiver = new ptmp::TPReceiver( ptmp_util::make_ptmp_socket_string("SUB","connect",recvr_sock) );
  ptmp::data::TPSet SetReceived;

  while(!stopping_flag_.load()) {

    bool received = (*receiver)(SetReceived, timeout_); 

    if(!received)  ++norecvds_;
    else          ++n_recvds_;

    nTPhits_+=SetReceived.tps_size();

    if(prev_counts_!=0 && (SetReceived.count()!=prev_counts_+1)){
      // Somehow signal that we missed an item on this input
    }
    prev_counts_=SetReceived.count();

    // If we didn't get a set don't do any more this round
    if(!received) { continue; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
                                                                
    ++n_recvd_;
                                                                
    if ((n_recvd_ < prescale_) && faketrigger_) continue;
                                                                
    n_recvd_ = 0;
    ++ntriggers_;
                                                                
    if(!timestamp_queue_.write(SetReceived.tstart())) ++fqueue_;
    
  }
    
  DAQLogger::LogInfo(instance_name_) << "Stop called, ending TPSet handler thread.";
  delete receiver;
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  DAQLogger::LogInfo(instance_name_) << "Ended TP receiver";

}

void dune::SWTrigger::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";

  stopping_flag_.store(true);   // We do want this here, if we don't use an
  // atomic<int> for stopping_flag_ (see header file comments)

  // Shut it all down
  if(!faketrigger_) tdGen_.reset(nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  tczipper_.reset(nullptr);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  DAQLogger::LogInfo(instance_name_) << "Shutdown TPZipper and TPFilter.";

  DAQLogger::LogInfo(instance_name_) << "Joining threads.";
  tpset_handler.join();
  metricsThread.join();
  ts_subscriber_->join();
  DAQLogger::LogInfo(instance_name_) << "Threads joined.";

  std::ostringstream ss_stats;
  ss_stats << "Statistics by input link:" << std::endl;

  ss_stats << "Number of TP(TC)Sets not received ";
  ss_stats << std::setw(7) << norecvds_;
  ss_stats << std::endl;

  ss_stats << "Number of TP(TC)Sets received     ";
  ss_stats << std::setw(7) << n_recvds_;
  ss_stats << std::endl;

  ss_stats << "Number of TrigPrims received      ";
  ss_stats << std::setw(7) << nTPhits_;
  ss_stats << std::endl;

  ss_stats << "Final TP(TC)Set count()           ";
  ss_stats << std::setw(7) << prev_counts_;
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
