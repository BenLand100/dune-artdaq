//################################################################################
//# /*
//#        TimingReceiverDebug_generator.cpp
//#
//#  Giles Barr, Justo Martin-Albo, Farrukh Azfar, Philip Rodrigues
//#  Jan 2017,  May 2017, Mar 2018
//#  for ProtoDUNE
//# */
//################################################################################


#include "artdaq/DAQdata/Globals.hh"
#define TRACE_NAME (app_name + "_TimingReceiverDebug").c_str()
#define TLVL_HWSTATUS 20
#define TLVL_TIMING 10

#include "dune-artdaq/Generators/TimingReceiverDebug.hh"
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


//#include "timingBoard/InhibitGet.h" // The interface to the ZeroMQ trigger inhibit master
// #include "timingBoard/StatusPublisher.hh"
#include "timingBoard/FragmentPublisher.hh"
// #include "timingBoard/HwClockPublisher.hh"

#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push

#include "pdt/PartitionNode.hpp" // The interface to a timing system
// partition, from the
// protodune_timing UPS product

#pragma GCC diagnostic pop

#include "artdaq/Application/BoardReaderCore.hh"

using namespace dune;
using namespace uhal;

// SetUHALLog is an attempt at a fiddle.  It is a class that does nothing except that it's
// constructor calls the uhal singleton to change the logging level to Notice.   The problem
// is that otherwise, when the constructor for the TimingReceiverDebug is called, it creates the
// connection manager before we have the opportunity to use the singleton, so the connection
// managers creation happens at full verbosity!   By listing the SetUHALLog logFiddle_ before
// the connectionManager_ in the class definition for TimingReceiverDebug, we can make it call
// in the right order.
dune::SetUHALLog::SetUHALLog(int) {
  uhal::setLogLevelTo(uhal::Notice());
}

// Constructor ------------------------------------------------------------------------------
dune::TimingReceiverDebug::TimingReceiverDebug(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps)
  , instance_name_("TimingReceiverDebug")
  , stopping_flag_(0)
  , throttling_state_(0)
  , stopping_state_(0)
  //  , inhibitget_timer_(ps.get<uint32_t>("inhibit_get_timer", 5000000))  // 3 secs TODO: Should make this a ps.get()
  , logFiddle_(0)            // This is a fudge, see explanation above
  , connectionsFile_(ps.get<std::string>("connections_file", "/home/artdaq1/giles/a_vm_mbp/dune-artdaq/Generators/timingBoard/connections.xml"))
  , bcmc_( "file://" + connectionsFile_ )  // a string (non-const)
  , connectionManager_(bcmc_)
  , hw_(connectionManager_.getDevice(ps.get<std::string>("hardware_select", "DUNE_SECONDARY")))
  , partition_number_(ps.get<uint32_t>("partition_number", 0))
  , debugprint_(ps.get<uint32_t>("debug_print", 0))
  , trigger_mask_(ps.get<uint32_t>("trigger_mask", 0xff))
  , end_run_wait_(ps.get<uint32_t>("end_run_wait", 1000))
  , enable_spill_commands_(ps.get<bool>("enable_spill_commands", false))
  , enable_spill_gate_(ps.get<bool>("enable_spill_gate", false))
  //  , zmq_conn_(ps.get<std::string>("zmq_connection", "tcp://pddaq-gen05-daq0:5566"))
  //  , zmq_conn_out_(ps.get<std::string>("zmq_connection_out", "tcp://*:5599"))
      , zmq_fragment_conn_out_(ps.get<std::string>("zmq_fragment_connection_out", "tcp://*:7123"))
  //  , zmq_hwtimer_conn_out_(ps.get<std::string>("zmq_hwtimer_conn_out", "tcp://*:5555"))
  , valid_firmware_versions_fcl_(ps.get<std::vector<int>>("valid_firmware_versions", std::vector<int>()))
  , want_inhibit_(false)
  , propagate_trigger_(ps.get<uint32_t>("generated_fragments_per_event", 1))
  , last_spillstart_tstampl_(0xffffffff) // Timestamp of most recent start-of-spill
  , last_spillstart_tstamph_(0xffffffff) // ...
  , last_spillend_tstampl_(0xffffffff)  // Timestamp of most recent end-of-spill
  , last_spillend_tstamph_(0xffffffff)  // ...
  , last_runstart_tstampl_(0xffffffff)  // Timestamp of most recent start-of-run
  , last_runstart_tstamph_(0xffffffff)  // ...
  , send_fragments_(ps.get<bool>("send_fragments", false))
{

  DAQLogger::LogWarning(instance_name_) << "JCF, Feb-28-2020: this is a variant of the TimingReceiver fragment generator which has been modified so that it doesn't issue any pdtbutler commands, but tries to read out data if data exists.";

  // TODO:
  // AT: Move hardware interface creation in here, for better exception handling

    auto rmConfig = ps.get<fhicl::ParameterSet>("routing_table_config", fhicl::ParameterSet());
    use_routing_master_ = rmConfig.get<bool>("use_routing_master", false);
    TLOG(TLVL_DEBUG) << "use_routing_master = " << use_routing_master_;

    // PAR 2018-03-09: We don't do any pre-partition-setup steps in
    // the board reader any more. They're done by the butler. I'm
    // leaving the comment below for posterity though
    //
    // Dave N's message part 1:
    // The pre-partition-setup status needs to be:
    //
    // - Board set up (clocks running and checked, etc)  [in hw_init, with initsoftness_=0]
    // - Timestamp set (if we ever do time-of-day stuff) [not done yet]
    // - Triggers from trigger enabled and gaps set      [not done yet]
    // - Command generators set up                       [done here]
    // - Spills or fake spills enabled                   [wait for firmware upgrade]

    //  if (inhibitget_timer_ == 0) inhibitget_timer_ = 2000000000;  // Zero inhibitget_timer waits infinite.

  // PAR 2018-03-09: We don't do any pre-partition-setup steps in
  // the board reader any more. They're done by the butler. I'm
  // leaving the comment below for posterity though
  //
  // Dave N's message part 1:
  // The pre-partition-setup status needs to be:
  //
  // - Board set up (clocks running and checked, etc)  [in hw_init, with initsoftness_=0]
  // - Timestamp set (if we ever do time-of-day stuff) [not done yet]
  // - Triggers from trigger enabled and gaps set      [not done yet]
  // - Command generators set up                       [done here]
  // - Spills or fake spills enabled                   [wait for firmware upgrade]


  if (ps.has_key("run_trigger_output")) {
    trigger_outfile_dir_ = ps.get<std::string>( "run_trigger_output") ;
    if ( trigger_outfile_dir_.back() != '/' ) trigger_outfile_dir_ += '/' ;
  } else {
    trigger_outfile_dir_ = "";
  }

  // // AT: Ensure that the hardware is up and running.
  // // Check that the board is reachable
  // // Read configuration version
  // ValWord<uint32_t> lVersion = hw_.getNode("master_top.master.global.version").read();
  // hw_.dispatch();

  // // Match Fw version with configuration
  // DAQLogger::LogInfo(instance_name_) << "Timing Master firmware version " << std::showbase << std::hex << (uint32_t)lVersion;
  // std::set<uint32_t> allowed_fw_versions{0x050001};
  // for (auto const& ver : valid_firmware_versions_fcl_) allowed_fw_versions.insert(ver);

  // if ( allowed_fw_versions.find(lVersion) == allowed_fw_versions.end() ) {
  //   std::stringstream errormsg;
  //   errormsg << "Firmware version mismatch! Got firmware version " << std::showbase << std::hex << lVersion << " but allowed versions are:";
  //   for (auto allowed_ver : allowed_fw_versions) {
  //     errormsg << std::showbase << std::hex << allowed_ver << " ";
  //   }
  //   DAQLogger::LogError(instance_name_) << errormsg.str();
  // }

  // // Measure the input clock frequency
  // hw_.getNode("io.freq.ctrl.chan_sel").write(0);
  // hw_.getNode("io.freq.ctrl.en_crap_mode").write(0);
  // hw_.dispatch();
  // std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  // uhal::ValWord<uint32_t> fq = hw_.getNode("io.freq.freq.count").read();
  // uhal::ValWord<uint32_t> fv = hw_.getNode("io.freq.freq.valid").read();
  // hw_.dispatch();


  // float lFrequency = ((uint32_t)fq) * 119.20928 / 1000000;
  // DAQLogger::LogInfo(instance_name_) << "Measured timing master frequency: " << lFrequency << " Hz";

  // // The TLU has a 50 MHz clock, and other hardware has a 250 MHz
  // // clock. Make sure we're in one of those ranges
  // if ( !( (lFrequency > 240. && lFrequency < 260) ||
  //         (lFrequency >  49. && lFrequency <  51) ) ) {
  //   DAQLogger::LogError(instance_name_) << "Timing master clock out of expected range: " << lFrequency << " Hz. Has it been configured?";
  // }

  // // Dave N's message part 2:
  // // So a likely cold-start procedure for the partition (not the board!) would be:
  // //
  // // - Disable the partition, just in case             [done here]
  // // - Disable triggers                                [done here, repeated at start()]
  // // - Disable buffer                                  [done here, repeated at start()]
  // // - Enable the partition                            [done here, repeated at start()]
  // // - Set the command mask                            [done here, repeated at start()]
  // // - Enable triggers                                 [Not done until inhibit lifted (in get_next_())]

  // // Enable/disable the sending of spill start/stop
  // uint32_t fragment_mask = 0;
  // if (enable_spill_commands_) {
  //   fragment_mask |= (1 << (int)dune::TimingCommand::RunStart);
  //   fragment_mask |= (1 << (int)dune::TimingCommand::SpillStart);
  //   fragment_mask |= (1 << (int)dune::TimingCommand::SpillStop);
  // }
  // DAQLogger::LogInfo(instance_name_) << "Setting fragment mask to " << std::showbase << std::hex << fragment_mask;
  // master_partition().getNode("csr.ctrl.frag_mask").write(fragment_mask);
  // master_partition().getClient().dispatch();

  // // Modify the trigger mask so we only see fake triggers in our own partition
  // fiddle_trigger_mask();

  // arguments to enable() are whether to enable and whether to
  // dispatch() respectively
  master_partition().enable(0, true);
  master_partition().stop();
  master_partition().enable(1, true);

  DAQLogger::LogInfo(instance_name_) << "This code is designed for the Iceberg teststand rather than ProtoDUNE, so enable_spill_gate should be 0/false. Checking its actual value: " << enable_spill_gate_;
  master_partition().configure(trigger_mask_, enable_spill_gate_);

  // // Set up connection to Inhibit Master. This is the inbound
  // // connection (ie, InhibitMaster talks to us to say whether we
  // // should enable triggers). We'll actually call
  // // InhibitGet_connect() in start(), in order to do so as late as
  // // possible and minimize the chance of reading stale messages from
  // // the InhibitMaster
  // //  InhibitGet_init(inhibitget_timer_);

  // // Set up outgoing connection to InhibitMaster: this is where we
  // // broadcast whether we're happy to take triggers
  // //  status_publisher_.reset(new artdaq::StatusPublisher(instance_name_, zmq_conn_out_));
  // //  status_publisher_->BindPublisher();

  if(propagate_trigger_) {
    fragment_publisher_.reset(new artdaq::FragmentPublisher(zmq_fragment_conn_out_));
    fragment_publisher_->BindPublisher();
  }
  // //  hwtime_publisher_.reset(new artdaq::HwClockPublisher(zmq_hwtimer_conn_out_ ));
  // //  hwtime_publisher_->bind();

  // TODO: Do we really need to sleep here to wait for the socket to bind?
  usleep(2000000);
  DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";
}


// // ----------------------------------------------------------------------------
// void dune::TimingReceiverDebug::publishHwTime() {
//   while( !hwclock_publisher_stop_ ) {
//       uhal::ValVector<uint32_t> ts_enc = hw_.getNode("master_top.master.tstamp.ctr.val").readBlock(2);
//       hw_.dispatch();
//       uint64_t ts = ((uint64_t)ts_enc[1] << 32) + ts_enc[0];
//       DAQLogger::LogInfo(instance_name_) << "Retrieved hw timestamp " << ts;
//       this->hwtime_publisher_->publish(ts);
//       std::this_thread::sleep_for(std::chrono::milliseconds(1500));
//   }

//   DAQLogger::LogInfo(instance_name_) << "HwClock Publisher shutting down";
// }


// // ----------------------------------------------------------------------------
// void dune::TimingReceiverDebug::startHwTimePublisher() {

//   DAQLogger::LogInfo(instance_name_) << "Starting HwClock Publisher ";

//   hwclock_publisher_stop_ = false;
//   hwclock_publisher_thread_ = std::thread(&dune::TimingReceiverDebug::publishHwTime, this);
// }


// // ----------------------------------------------------------------------------
// void dune::TimingReceiverDebug::stopHwTimePublisher() {
//   hwclock_publisher_stop_ = true;
//   hwclock_publisher_thread_.join();
// }


// start() routine --------------------------------------------------------------------------
void dune::TimingReceiverDebug::start(void) {
  DAQLogger::LogDebug(instance_name_) << "start() called\n";


  // See header file for meanings of these variables
  stopping_flag_    = 0;
  stopping_state_   = 0;
  throttling_state_ = 1;    // 0 Causes it to start triggers immediately, 1 means wait for InhibitMaster to release

  // master.global.csr.ctrl.part_sel   set to partition_    (not implemented yet)

  // First repeat the enabling part of Dave N's message part 2
  // - Enable the partition                            [done here]
  // - Set the command mask                            [done here]

  // These are the steps taken by pdtbutler's `configure` command

  master_partition().reset();
  master_partition().configure(trigger_mask_, enable_spill_gate_);
  master_partition().enable(1);

  // Dave N's message part 3
  // To start a run (assuming that no data has been read out between runs):
  //
  // - Disable buffer                                  [done by master_partition().start()]
  // - Enable buffer                                   [done by master_partition().start()]
  // - Set run_req                                     [done by master_partition().start()]
  // - Wait for run_stat to go high                    [done by master_partition().start()]


  try {
    master_partition().start();
  } catch (pdt::RunRequestTimeoutExpired& e) {
    DAQLogger::LogError(instance_name_) << "A pdt::RunRequestTimeoutExpired exception was thrown on start";
   }

  //  InhibitGet_connect(zmq_conn_.c_str());
  //  InhibitGet_retime(inhibitget_timer_);

  //  this->reset_met_variables(false);

  //  this->startHwTimePublisher();
}


// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::stop(void) {
  DAQLogger::LogInfo(instance_name_) << "stop() called";

  stopping_flag_ = 1;   // We do want this here, if we don't use an
  // atomic<int> for stopping_flag_ (see header
  // file comments)

  if (trigger_outfile_dir_ != "") {
    std::stringstream outfile_name;
    outfile_name << trigger_outfile_dir_  << "run_" << run_number() << "_timing_board_triggers.txt";
    std::ofstream out( outfile_name.str() ) ;

    std::vector<std::string> commandNames = {
      "TimeSync",
      "Echo",
      "SpillStart",
      "SpillStop",
      "RunStart",
      "RunStop",
      "WibCalib",
      "SSPCalib",
      "FakeTrig0",
      "FakeTrig1",
      "FakeTrig2",
      "FakeTrig3",
      "BeamTrig",
      "NoBeamTrig",
      "ExtFakeTrig"
    };

    for (size_t i = 0; i < commandNames.size(); ++i) {
      out << commandNames[i] << "\t";
      if (met_accepted_trig_count_.size() > i) {
        out << met_accepted_trig_count_.at(i) << "\t";
      } else {
        out << "0\t";
      }
      if (met_rejected_trig_count_.size() > i) {
        out << met_rejected_trig_count_.at(i) << "\t";
      } else {
        out << "0\t";
      }
      out << std::endl;
    }
  }

  //  this->stopHwTimePublisher();

}


// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::stopNoMutex(void) {
  DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";

  // (PAR 2018-10-15) Change this to *not* set stopping_flag_: that
  // variable is a bare int (not std::atomic) and stopnoMutex() is
  // called from a different thread from getNext_(). So this means
  // that stopping_flag_ can change in the middle of a getNext_()
  // call, which would break the logic of getNext_() and may be
  // causing the "getNext: std::exception caught: Failed to stop
  // after 5000 milliseconds" errors.
  //
  // This flag is instead set in stop(), which is guaranteed to not
  // be called while getNext_() is running

  // stopping_flag_ = 1;    // Tells the getNext_() while loop to stop the run
}

// ----------------------------------------------------------------------------
bool dune::TimingReceiverDebug::checkHWStatus_()
{

  //  DAQLogger::LogInfo(instance_name_) << "JCF, Feb-21-2020: Skipping call of checkHWStatus";
  // static bool first=true;
  // if(first){
  //   DAQLogger::LogInfo(instance_name_) << "First call of checkHWStatus_()";
  //   first=false;
  // }

  // auto dsmptr = artdaq::BoardReaderCore::GetDataSenderManagerPtr();
  // if(!dsmptr)
  //   TLOG(TLVL_HWSTATUS) << "DataSenderManagerPtr not valid.";

  // auto mp_ovrflw = master_partition().readROBWarningOverflow();
  // int n_remaining_table_entries = dsmptr? dsmptr->GetRemainingRoutingTableEntries() : -1;
  // int n_table_count = dsmptr? dsmptr->GetRoutingTableEntryCount() : -1;
  // TLOG(TLVL_HWSTATUS) << "hwstatus: buf_warn=" << mp_ovrflw
  //                     << " table_count=" << n_table_count
  //                     << " table_entries_remaining=" << n_remaining_table_entries;

  // bool new_want_inhibit=false;
  // std::string status_msg="";

  // if(mp_ovrflw){
  //   // Tell the InhibitMaster that we want to stop triggers, then
  //   // carry on with this iteration of the loop
  //   DAQLogger::LogInfo(instance_name_) << "buf_warn is high, with " << master_partition().numEventsInBuffer() << " events in buffer. Requesting InhibitMaster to stop triggers";
  //   status_msg="ROBWarningOverflow";
  //   new_want_inhibit=true;
  // }
  // //check if there are available routing tokens, and if not inhibit
  // else if (use_routing_master_ && n_remaining_table_entries == 0) {
  //   new_want_inhibit=true;
  //   status_msg="NoAvailableTokens";
  // }
  // else{
  //   new_want_inhibit=false;
  // }

  // // if (new_want_inhibit && !want_inhibit_) {
  // //   DAQLogger::LogInfo(instance_name_) << "Want inhibit Status change: Publishing bad status: \"" << status_msg << "\"";
  // //   status_publisher_->PublishBadStatus(status_msg);
  // // }
  // // if (want_inhibit_ && !new_want_inhibit) {
  // //   DAQLogger::LogInfo(instance_name_) << "Want inhibit status change: Publishing good status";
  // //   status_publisher_->PublishGoodStatus();
  // // }

  // // want_inhibit_ = new_want_inhibit;

  // // Check that the timing-trigger endpoint ("ext-trig" in the
  // // butler), which is listening to the trigger board, is is a ready
  // // state. It gets upset by errors on the return path and/or by the
  // // CTB firmware being reloaded. If it's not ready, and this
  // // partition is using external triggers (as specified by the
  // // trigger mask), then that's a fatal error, so we return false to
  // // tell artdaq to stop the run
  // //
  // // Extra complication: only the TLU has the necessary registers (the fanouts do not), so test that first
  // if (!hw_.getNodes("master_top.trig").empty()) {
  //   ValWord<uint32_t> trig_endpoint_ready = hw_.getNode("master_top.trig.csr.stat.ep_rdy").read();
  //   // The full status of the endpoint
  //   ValWord<uint32_t> trig_endpoint_status = hw_.getNode("master_top.trig.csr.stat.ep_stat").read();
  //   hw_.dispatch();
  //   if (trig_endpoint_ready.value() == 0 || trig_endpoint_status.value() > 0x8) {
  //     // Is this partition interested in external triggers? The lowest four triggers are internal; higher are external
  //     bool want_external = trigger_mask_ & 0xfffffff0;
  //     if (want_external) {
  //       DAQLogger::LogError(instance_name_)
  //           << "timing-trigger endpoint is not ready: status is "
  //           << std::showbase << std::hex << trig_endpoint_status.value()
  //           << " when trigger mask is "
  //           << std::showbase << std::hex << trigger_mask_
  //           << ". This is a fatal error";
  //       return false;
  //     }
  //   }
  // }


  return true;
}


//#define D(a) a
#define D(a)
#define E(a) a

// ----------------------------------------------------------------------------
bool dune::TimingReceiverDebug::getNext_(artdaq::FragmentPtrs &frags) {

  static size_t ncalls = 1;
 
  if (ncalls == 1) {
    DAQLogger::LogInfo(instance_name_) << "JCF, Mar-10-2020: sleeping for two seconds on first call to getNext_; will enable triggers on next call to getNext_";
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
  } else if (ncalls == 2) {
    DAQLogger::LogInfo(instance_name_) << "Enabling triggers";
    master_partition().enableTriggers(1);
  }

  ncalls++;

  if(!send_fragments_){
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (stopping_flag_ != 0) return false;
    else                     return true;
  }
  
  // GetNext can return in three ways:
  //  (1) We have some events [return true with events in the frags vector]
  //  (2) Normal case when no events happened to arrive [return true with
  //      empty frags vector]
  //  (3) Case at end of run, continue to return true until we are sure there
  //      are no more events, then return false
  // To avoid the 100% issue, for case (2), i.e. no events are available, this
  // routine should sleep for a bit

  // Notes (2017-03-09)
  // Check status register: is there data?
  // We may need to check again after certain amount of time
  int counter = 0;
  int max_counter = 3; // this should be sensible N;
  int max_timeout = 50;  // millisecs

  // Technique: Use a generic loop (i.e. exit is from break or return statements,
  // not from a while (condition).  This allows us to easily reorder the sequence
  // of actions to get them right
  while (true) {
    int timeout = max_timeout;  // If no event found we wait this time

    // Check for stop run
    if (stopping_state_ == 0 && stopping_flag_ != 0) {
      DAQLogger::LogInfo(instance_name_) << "getNext_ saw stopping_flag with value " << (uint32_t)stopping_flag_;
      // Stop the run, disable buffer and triggers
      master_partition().stop();

      hw_.dispatch();
      DAQLogger::LogInfo(instance_name_) << "Partition stopped. Waiting " << end_run_wait_ << "us for straggling events";
      throttling_state_ = 0;  // Note for now, we remove XOFF immediately at end
      // of run, discuss with hw people if that is right.
      stopping_state_ = 1;    // HW has been told to stop, may have more data
      usleep(end_run_wait_);  // Wait the max time spec for hardware to push more events
      // after a run has stopped
    }

    // We send goodness/badness every go round of the getNext_()
    // loop. Wes says this is fine (no need to only send on
    // change). Also, we're safe from rapidly oscillating between warn
    // and not-warn because the register in the timing board has some
    // hysteresis
    /*
    //
    //  Wes testing moving this to checkHWStatus_
    //
    */
    auto numEventsInBuffer = master_partition().numEventsInBuffer();
    unsigned int havedata = (numEventsInBuffer > 0); // Wait for a complete event
    DAQLogger::LogDebug(instance_name_) << "numEventsInBuffer == " << numEventsInBuffer << ", havedata == " << havedata;
    if (havedata) timeout = 1;  // If we have data, we don't wait around in case there is more or a throttle

    TLOG(TLVL_TIMING) << "havedata: " << havedata;
    // TODO: Consider moving throttling check up here; as it is, we could receive a huge rate of events even though we
    // are also being told to throttle, and may not notice that throttling request.

    if (havedata) {   // An event or many events have arrived
      while (havedata) {

        // Make a fragment.  Follow the way it is done in the SSP
        // boardreader. We always form fragments, even for the events
        // that we're not going to send out to artdaq (spill
        // start/end, run start), mostly to get the timestamp
        // calculation done in the fragment, but partly to make the
        // code easier to follow(?)

	DAQLogger::LogInfo(instance_name_) << "Creating fragment";
        std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( TimingFragment::size() * sizeof(uint32_t),
                                              artdaq::Fragment::InvalidSequenceID,
                                              artdaq::Fragment::InvalidFragmentID,
                                              artdaq::Fragment::InvalidFragmentType,
                                              dune::TimingFragment::Metadata(TimingFragment::VERSION));
        // It's unclear to me whether the constructor above actually sets the metadata, so let's do it here too to be sure
        f->updateMetadata(TimingFragment::Metadata(TimingFragment::VERSION));
        // Read the data from the hardware
        std::vector<uint32_t> uwords = master_partition().readEvents(1);

#ifdef SEPARATE_DEBUG
        uint32_t word[12];  // Declare like this if we want a local variable instead of putting it straight in the fragmenti
#else
        uint32_t* word = reinterpret_cast<uint32_t *> (f->dataBeginBytes());    // dataBeginBytes returns a byte_t pointer
#endif
        for (int i = 0; i < 6; i++) {  word[i] = uwords[i]; } // Unpacks from uHALs ValVector<> into the fragment

        // Set the last spill/run timestamps in the fragment

        // There had better be enough space in the object. If not, something has gone horribly wrong
        static_assert(TimingFragment::Body::size >= 12ul);

        // These must be kept in sync with the order declared in TimingFragment::Body in TimingFragment.hh
        word[6] = last_runstart_tstampl_;
        word[7] = last_runstart_tstamph_;

        word[8] = last_spillstart_tstampl_;
        word[9] = last_spillstart_tstamph_;

        word[10] = last_spillend_tstampl_;
        word[11] = last_spillend_tstamph_;

        dune::TimingFragment fo(*f);    // Overlay class - the internal bits of the class are
        // accessible here with the same functions we use offline.

        // Print out, can do it via the overlay class or with raw access
#ifdef SEPARATE_DEBUG
        // This way of printing also works in non-SEPARATE_DEBUG
        printf("Data: 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n"
               , word[0], word[1], word[2], word[3], word[4], word[5]);
#else
        if (debugprint_ > 2) {
          DAQLogger::LogDebug(instance_name_) << fo;
        }
#endif

        TLOG(TLVL_TIMING) << fo;
        // Fill in the fragment header fields (not some other fragment generators may put these in the
        // constructor for the fragment, but here we push them in one at a time.
        f->setSequenceID( ev_counter() );  // ev_counter is in our base class  // or f->setSequenceID(fo.get_evtctr())
        f->setFragmentID( fragment_id() ); // fragment_id is in our base class, fhicl sets it
        f->setUserType( dune::detail::TIMING );
        //  No metadata in this block

        DAQLogger::LogInfo(instance_name_) << "For timing fragment with sequence ID " << ev_counter() << ", scmd " << std::showbase << std::hex << fo.get_scmd() << std::dec <<  ", setting the timestamp to " << fo.get_tstamp();
        f->setTimestamp(fo.get_tstamp());  // 64-bit number


        // Do we want to send the fragment out to ZeroMQ and artdaq? We *don't* send out run start, spill start and spill end fragments.
        // TODO: maybe this check should just be whether the scmd is >=0x8
        dune::TimingCommand commandType = (dune::TimingCommand)fo.get_scmd();
        bool shouldSendFragment = !(commandType == dune::TimingCommand::RunStart   ||
                                    commandType == dune::TimingCommand::RunStop    ||
                                    commandType == dune::TimingCommand::SpillStart ||
                                    commandType == dune::TimingCommand::SpillStop);

        if (shouldSendFragment) {
	  // Send the fragment out on ZeroMQ for FELIX and whoever else wants to listen for it
	  int pubSuccess = fragment_publisher_->PublishFragment(f.get(), &fo);
	  if (!pubSuccess)
	    DAQLogger::LogWarning(instance_name_) << "Publishing fragment to ZeroMQ failed";

          frags.emplace_back(std::move(f));
          // We only increment the event counter for events we send out
          ev_counter_inc();
        } else {
          DAQLogger::LogInfo(instance_name_) << "Got a command of type " << std::showbase << std::hex << fo.get_scmd() << " with timestamp " << fo.get_tstamp() << " which we won't send";
        }

        // Update the last spill/run timestamps if the fragment type is one of those
        switch (commandType) {
        case dune::TimingCommand::RunStart:
          last_runstart_tstampl_ = fo.get_tstampl();
          last_runstart_tstamph_ = fo.get_tstamph();
          break;
        case dune::TimingCommand::SpillStart:
          last_spillstart_tstampl_ = fo.get_tstampl();
          last_spillstart_tstamph_ = fo.get_tstamph();
          break;
        case dune::TimingCommand::SpillStop:
          last_spillend_tstampl_ = fo.get_tstampl();
          last_spillend_tstamph_ = fo.get_tstamph();
          break;
        default:
          break;
        };

        havedata = false;    // Combined with the while above, we could make havedata an integer
        // and read multiple events at once (so use "havedata -= 6;")

        this->update_met_variables(fo);

        // TODO:  Update when we know what the spill start type is
        if (fo.get_scmd() == 0) this->reset_met_variables(true);  // true = only per-spill variables

      }  // end while
    } else if (stopping_state_ > 0) { // We now know there is no more data at the stop
      stopping_state_ = 2;            // This causes return to be false (no more data)
      // We used to disable the buffer here, but Dave on slack, 2018-03-08:
      // """
      // Likewise, not clear why we’d ever want to run with buf_en
      // disabled, it’s really there so you can make sure the buffer
      // is clear at run start
      //
      // ie. toggle it low then high before starting a run
      // """
      //
      // So now we don't disable the buffer
      DAQLogger::LogInfo(instance_name_) << "Fragment generator returning clean run stop and end of data";
      break;
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
      //usleep(timeout);
    }

    // Check for throttling change
    // // Do not allow a throttling change after the run has been requested to stop
    // do {     // do {} while(false); allows pleasing use of break to get out without lots of nesting
    //   if (stopping_flag_ != 0) break;      // throttling change not desired after run stop request.
    //   uint32_t tf = InhibitGet_get();      // Can give 0=No change, 1=OK, 2=Not OK)
    //   uint32_t bit = 1;                    // If we change, this is the value to set. 1=running
    //   TLOG(TLVL_TIMING) << "Received value " << tf << " from InhibitGet_get()\n";
    //   if (tf == 0) break;                  // No change, so no need to do anything


    //   if (tf == 1) {                       // Want to be running
    //     if (throttling_state_ != 1) break; // No change needed
    //     bit = 1;                           // To set running (line not needed, bit is set to 1 already)
    //   } else if (tf == 2) {                // Want to be stopped
    //     if (throttling_state_ == 1) break; // Already stopped, no change needed
    //     bit = 0;                           // To set stopped
    //   } else {                             // Should never happen, InhibitGet_get returned unknown value.
    //     DAQLogger::LogWarning(instance_name_) << "TimingReceiverDebug_generator.cc: Logic error should not happen";
    //     break;                             // Treat as no change
    //   }
    //   std::stringstream change_msg;
    //   change_msg << "Throttle state change: Writing " << bit
    //              << " to trig_en.  [Throttling state was " << throttling_state_
    //              << "]\n";
    //   TLOG(TLVL_TIMING) << change_msg.str();
    //   if (debugprint_ > 0) {
    //     DAQLogger::LogInfo(instance_name_) << change_msg.str();
    //   }
    //   master_partition().enableTriggers(bit); // Set XOFF or XON as requested
    //   throttling_state_ = bit ^ 0x1;       // throttling_state is the opposite of bit
    // } while (false);                       // Do loop once only (mainly to have lots of 'break's above)

    // Limit the number of tests we do before returning
    if (!(counter < max_counter)) break;
    ++counter;
  }

  this->send_met_variables();

  if (stopping_flag_ != 0) {
    static int nStoppingLoops = 0;
    ++nStoppingLoops;
    DAQLogger::LogInfo(instance_name_) << "Completed " << nStoppingLoops << " getNext_() loops while stopping_flag was high";
  }

  if (stopping_state_ == 2) return false;    // stopping_state = 2 means we know there is no more data
  return true;   // Note: This routine can return (with a false) from inside loop (when
  // run has stopped and we know there is no more data)
}

// ----------------------------------------------------------------------------
bool dune::TimingReceiverDebug::startOfDatataking() { return true; }


// ----------------------------------------------------------------------------
// Local "private" methods
const pdt::PartitionNode& dune::TimingReceiverDebug::master_partition() {
  std::stringstream ss;
  ss << "master_top.master.partition" << partition_number_;
  return hw_.getNode<pdt::PartitionNode>(ss.str());
}

// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::fiddle_trigger_mask() {
  // There is a different fake trigger command for each partition, as follows:

  // Partition        Command
  // ------------------------
  //         0            0x8
  //         1            0x9
  //         2            0xa
  //         3            0xb

  // The trigger mask applies starting at 0x8, so eg mask 0x1 turns
  // on triggers in partition 0 only

  uint32_t old_mask = trigger_mask_;
  // We modify the trigger mask to always be zero for the _other_ partitions
  trigger_mask_ &= 0xf0 | (1 << partition_number_);
  DAQLogger::LogInfo(instance_name_) << "fiddle_trigger_mask partn " << partition_number_
                                     << " Old: " << std::showbase << std::hex << old_mask
                                     << " New: " << std::showbase << std::hex << trigger_mask_;
}


// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::reset_met_variables(bool onlyspill) {

  // If onlyspill, only reset the in-spill variables ...
  met_sevent_ = 0;
  met_sintmin_ = 2000000000;
  met_sintmax_ = 0;
  if (onlyspill) return;

  // ... otherwise reset them all
  met_event_ = 0;
  met_tstamp_ = 0;
  met_rintmin_ = met_sintmin_;
  met_rintmax_ = met_sintmax_;
}


// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::update_met_variables(dune::TimingFragment& fo) {

  // Update variables for met
  met_event_ = fo.get_evtctr();
  uint64_t told = met_tstamp_;   // Previous time stamp
  met_tstamp_ = fo.get_tstamp();
  int64_t diff = met_tstamp_ - told;
  if (diff >  2000000000) diff =  2000000000;  // Make value fit in int32 for metric
  if (diff < -2000000000) diff = -2000000000;
  met_sevent_++;
  if (met_rintmin_ > diff) met_rintmin_ = diff;
  if (met_sintmin_ > diff) met_sintmin_ = diff;
  if (met_rintmax_ < diff) met_rintmax_ = diff;
  if (met_sintmax_ < diff) met_sintmax_ = diff;
  // Get the accepted and rejected trigger counters
  //pdt::PartitionCounts trigCounts = master_partition().readCommandCounts();
  //  met_accepted_trig_count_ = trigCounts.accepted;
  //  met_rejected_trig_count_ = trigCounts.rejected;
}

// ----------------------------------------------------------------------------
void dune::TimingReceiverDebug::send_met_variables() {

  // When running in standalone mode, there's no metric manager, so just skip this bit
  if (!artdaq::Globals::metricMan_) {
    return;
  }

// Metrics:  Parameters to the sendMetric call are:
//  (1) name, (2) value (string, int, double, float or long uint), (3) units,
//  (4) level, (5) bool accumulate (default true), (6) ... more with good defaults

// All our variables should be int, except the timestamp which is long uint
  artdaq::Globals::metricMan_->sendMetric("TimingSys Events",            met_event_,  "events", 1, artdaq::MetricMode::LastPoint);
  const long unsigned int ts = met_tstamp_;
  artdaq::Globals::metricMan_->sendMetric("TimingSys TimeStamp",         ts,           "ticks", 1, artdaq::MetricMode::LastPoint);
  artdaq::Globals::metricMan_->sendMetric("TimingSys Events spill",      met_sevent_, "events", 1, artdaq::MetricMode::LastPoint);
  artdaq::Globals::metricMan_->sendMetric("TimingSys MinInterval run",   met_rintmin_, "ticks", 1, artdaq::MetricMode::LastPoint);
  artdaq::Globals::metricMan_->sendMetric("TimingSys MinInterval spill", met_sintmin_, "ticks", 1, artdaq::MetricMode::LastPoint);
  artdaq::Globals::metricMan_->sendMetric("TimingSys MaxInterval run",   met_rintmax_, "ticks", 1, artdaq::MetricMode::LastPoint);
  artdaq::Globals::metricMan_->sendMetric("TimingSys MaxInterval spill", met_sintmax_, "ticks", 1, artdaq::MetricMode::LastPoint);

  std::vector<std::string> commandNames = {
    "TimeSync",
    "Echo",
    "SpillStart",
    "SpillStop",
    "RunStart",
    "RunStop",
    "WibCalib",
    "Trig0",
    "Trig1",
    "Trig2",
    "Trig3",
    "Trig4",
    "Trig5",
    "Trig6",
    "Trig7",
    "Trig8"
  };

  std::stringstream acc_msg, rej_msg;
  acc_msg << "Acc. counts: ";
  rej_msg << "Rej. counts: ";

  // send the trigger counts
  for (size_t i = 0; i < 16; ++i) {
    if (met_accepted_trig_count_.size() > i) {
      std::stringstream ss1;
      ss1 << "TimingSys Accepted " << commandNames.at(i);
      artdaq::Globals::metricMan_->sendMetric(ss1.str(), (int)met_accepted_trig_count_.at(i), "triggers", 1, artdaq::MetricMode::LastPoint);
      acc_msg << ((int)met_accepted_trig_count_.at(i)) << ",";
    }

    if (met_rejected_trig_count_.size() > i) {
      std::stringstream ss2;
      ss2 << "TimingSys Rejected " << commandNames.at(i);
      artdaq::Globals::metricMan_->sendMetric(ss2.str(), (int)met_rejected_trig_count_.at(i), "triggers", 1, artdaq::MetricMode::LastPoint);
      rej_msg << ((int)met_rejected_trig_count_.at(i)) << ",";
    }
  }

  uint64_t last_spillstart = ((uint64_t)last_spillstart_tstamph_ << 32) |  (uint64_t)last_spillstart_tstampl_;
  artdaq::Globals::metricMan_->sendMetric("TimingSys Last SpillStart", last_spillstart, "ticks", 1, artdaq::MetricMode::LastPoint);
  uint64_t last_spillend = ((uint64_t)last_spillend_tstamph_ << 32) |  (uint64_t)last_spillend_tstampl_;
  artdaq::Globals::metricMan_->sendMetric("TimingSys Last SpillEnd", last_spillend, "ticks", 1, artdaq::MetricMode::LastPoint);

  TLOG(TLVL_TIMING) << acc_msg.str();
  TLOG(TLVL_TIMING) << rej_msg.str();
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TimingReceiverDebug)
