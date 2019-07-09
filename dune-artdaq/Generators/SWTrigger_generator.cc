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

#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"
#include "dune-raw-data/Overlays/TriggerFragment.hh"

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
  ,receiver_1_( ptmp_util::make_ptmp_socket_string("SUB","connect",{ps.get<std::string>("receiver_1")}) ) //Corresponds to trigcand500
  ,receiver_2_( ptmp_util::make_ptmp_socket_string("SUB","connect",{ps.get<std::string>("receiver_2")}) ) //Corresponds to trigcand600
  ,timeout_(ps.get<int>("timeout"))
  ,n_recvd_(0)
  ,p_count_1_(0)
  ,p_count_2_(0)
  ,ntriggers_(0)
  ,norecvd_(0)
  ,n_recvd_1_(0)
  ,n_recvd_2_(0)
  ,nTPhits_(0)
  ,nTPset_recvd_(0)
  ,fqueue_(0)
  ,loops_(0)
  ,qtpsets_(0)
  ,count_(0)
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
  stopping_flag_    = false;
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
  while (!stopping_flag_) {
    latest_ts_ = ts_receiver_->receiveHardwareTimeStamp();
  }
}

// tpsetHandler() routine ------------------------------------------------------------------

void dune::SWTrigger::tpsetHandler() {

  DAQLogger::LogInfo(instance_name_) << "Starting TPSet handler thread.";

  while(!stopping_flag_) {

    ptmp::data::TPSet SetReceived_1;
    ptmp::data::TPSet SetReceived_2;

    bool received_1 = receiver_1_(SetReceived_1, timeout_);
    bool received_2 = receiver_2_(SetReceived_2, timeout_);

    if (!received_1 && !received_2) { ++norecvd_; continue; }

    unsigned int count_1 = SetReceived_1.count();
    unsigned int count_2 = SetReceived_2.count();

    if (count_1 != p_count_1_){
      ++n_recvd_1_;
      nTPhits_ += SetReceived_2.tps_size();
      ++n_recvd_;
    }

    if (count_2 != p_count_2_){
      ++n_recvd_2_; 
      nTPhits_ += SetReceived_2.tps_size();
      ++n_recvd_;
    }

    p_count_1_ = count_1;
    p_count_2_ = count_2;

    // 4400 gives about 5Hz triggers
    if (n_recvd_ < 4400) continue;

    n_recvd_ = 0;
    ++ntriggers_;

    // Add the TPsets to the queue to allow access from getNext
    // TODO add in receiver 2
    if (!queue_.isFull()) queue_.write(SetReceived_1);
    if (queue_.isFull()) ++fqueue_;

  }

  DAQLogger::LogInfo(instance_name_) << "Stop called, ending TPSet handler thread.";

}

void dune::SWTrigger::stop(void)
{
  DAQLogger::LogInfo(instance_name_) << "stop() called";

  stopping_flag_ = true;   // We do want this here, if we don't use an
  // atomic<int> for stopping_flag_ (see header file comments)

  DAQLogger::LogInfo(instance_name_) << "Number of TP(TC)Sets not received " << norecvd_; 
  DAQLogger::LogInfo(instance_name_) << "Number of TP(TC)Sets received APA5 " << n_recvd_1_ << " APA6 " << n_recvd_2_ << " total " << n_recvd_;
  DAQLogger::LogInfo(instance_name_) << "Number of triggers " << ntriggers_;
  DAQLogger::LogInfo(instance_name_) << "Final TP(TC)Set count APA5 " << p_count_1_ << " APA6 " << p_count_2_;
  DAQLogger::LogInfo(instance_name_) << "Number of TPsets received in getNext " << qtpsets_ << " in loops " << loops_;
  //Avg & Max time diff now() - tstart
  //DAQLogger::LogInfo(instance_name_) << "Number of triggers " << ntriggers_;
  //Avg & Max time diff now() - creation
  //DAQLogger::LogInfo(instance_name_) << "Number of triggers " << ntriggers_;
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

  auto start = std::chrono::high_resolution_clock::now();

  tpset_ = queue_.frontPtr();
  ++loops_;

  if (!tpset_) {
    return true;
  } else {
    ++qtpsets_;
    DAQLogger::LogInfo(instance_name_) << "Received TPset count " << tpset_->count() << "  " << tpset_;
    //previous_ts_ = std::max(SetReceived_1.tstart(),SetReceived_2.tstart());
    previous_ts_ = tpset_->tstart();
    queue_.popFront();
  } 

  // Check for stop run
  if (stopping_flag_) {
    DAQLogger::LogInfo(instance_name_) << "getNext_ stopping ";
    return false;
  }

  uint32_t tf = InhibitGet_get();  
  if (tf==1) {
    throttling_state_=false;
  }
  else if (tf ==2) {
    throttling_state_= true;
  }
  if (throttling_state_) {
    return true;
  }

  
  auto ticks = std::chrono::duration_cast<std::chrono::duration<uint64_t, std::ratio<1,50000000>>>(std::chrono::system_clock::now().time_since_epoch());
  uint64_t now = ticks.count();
                                                                                                                                                         
  DAQLogger::LogInfo(instance_name_) << "Time difference between now and previous_ts " << now-previous_ts_;
  DAQLogger::LogInfo(instance_name_) << "Timestamp going to triggered fragment should be: " << previous_ts_;


//------------ Trigger Fragment -------------//
  std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( TriggerFragment::size()*sizeof(uint32_t),
      artdaq::Fragment::InvalidSequenceID,
      artdaq::Fragment::InvalidFragmentID,
      artdaq::Fragment::InvalidFragmentType,
      dune::TriggerFragment::Metadata(TriggerFragment::VERSION));
  // It's unclear to me whether the constructor above actually sets the metadata, so let's do it here too to be sure
  f->updateMetadata(TriggerFragment::Metadata(TriggerFragment::VERSION));

  // GLM: here get the trigger decisions based on hits

  uint32_t* triggerword = reinterpret_cast<uint32_t *> (f->dataBeginBytes());    // dataBeginBytes returns a byte_t pointer

  // Set the last spill/run timestamps in the fragment

  // There had better be enough space in the object. If not, something has gone horribly wrong
  static_assert(TriggerFragment::Body::size >= 19ul);

  // These must be kept in sync with the order declared in TriggerFragment::Body in TriggerFragment.hh
  triggerword[2]= previous_ts_&0xffffffff;
  triggerword[3]= (previous_ts_>>32);

  triggerword[6]=last_runstart_tstampl_;
  triggerword[7]=last_runstart_tstamph_;

  triggerword[8]=last_spillstart_tstampl_;
  triggerword[9]=last_spillstart_tstamph_;

  triggerword[10]=last_spillend_tstampl_;
  triggerword[11]=last_spillend_tstamph_;

  // Appended for trigger decision information
  triggerword[12]=count_;
  triggerword[13]=partition_number_;

  triggerword[14]=0;
  triggerword[15]=last_spillstart_tstampl_; // dummy
  triggerword[16]=last_spillstart_tstamph_; // dummy

  triggerword[17]=last_spillstart_tstampl_; // dummy
  triggerword[18]=last_spillend_tstamph_;   // dummy
  triggerword[19]=last_spillend_tstamph_;   // dummy




//---------- Timing Fragment -----------//
  
  uint32_t* timingword = reinterpret_cast<uint32_t *> (f->dataBeginBytes());    // dataBeginBytes returns a byte_t pointer

  // Set the last spill/run timestamps in the fragment

  // There had better be enough space in the object. If not, something has gone horribly wrong
  static_assert(TimingFragment::Body::size >= 12ul);

  // These must be kept in sync with the order declared in TimingFragment::Body in TimingFragment.hh
  timingword[2]= previous_ts_&0xffffffff;
  timingword[3]= (previous_ts_>>32);

  timingword[6]=last_runstart_tstampl_;
  timingword[7]=last_runstart_tstamph_;

  timingword[8]=last_spillstart_tstampl_;
  timingword[9]=last_spillstart_tstamph_;

  timingword[10]=last_spillend_tstampl_;
  timingword[11]=last_spillend_tstamph_;

  // Fill in the fragment header fields (not some other fragment generators may put these in the
  // constructor for the fragment, but here we push them in one at a time.
  f->setSequenceID( ev_counter() );  // ev_counter is in our base class  // or f->setSequenceID(fo.get_evtctr())
  f->setFragmentID( fragment_id() ); // fragment_id is in our base class, fhicl sets it
  f->setUserType( dune::detail::TIMING );
  //  No metadata in this block

  DAQLogger::LogInfo(instance_name_) << "For timing fragment with sequence ID " << ev_counter() << ", setting the timestamp to " << f->timestamp();
  f->setTimestamp(previous_ts_);  // 64-bit number

  // Send the fragment out on ZeroMQ for FELIX and whoever else wants to listen for it
  dune::TimingFragment fo(*f); 
  DAQLogger::LogInfo(instance_name_) << "Fragment timestamp: artdaq " << f->timestamp() << " timing : " << fo.get_tstamp();
  int pubSuccess = fragment_publisher_->PublishFragment(f.get(), &fo);
  if(!pubSuccess)
    DAQLogger::LogError(instance_name_) << "Publishing fragment to ZeroMQ failed";

  //Check how long is the time between receive/send of the PTMP message
  auto elapsed = std::chrono::high_resolution_clock::now() - start;
  long long microseconds = std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
  DAQLogger::LogInfo(instance_name_) << "Time from beginning of getNext until TPset published (usec): " << microseconds;

  frags.emplace_back(std::move(f));
  // We only increment the event counter for events we send out
  ev_counter_inc();

  return true;

}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::SWTrigger)
