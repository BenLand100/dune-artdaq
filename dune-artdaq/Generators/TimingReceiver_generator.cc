//################################################################################
//# /*
//#        TimingReceiver_generator.cpp
//#
//#  Giles Barr, Justo Martin-Albo, Farrukh Azfar, Jan 2017,  May 2017 
//#  for ProtoDUNE
//# */
//################################################################################

#include "dune-artdaq/Generators/TimingReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

//:#include "canvas/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/TimingFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
//:#include "artdaq-core/Utilities/SimpleLookupPolicy.h"
//:#include "messagefacility/MessageLogger/MessageLogger.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>

#include <unistd.h>
#include "timingBoard/TimingSequence.hpp"  // Defs of the sequence functions of uhal commands for setup etc.

#include "timingBoard/InhibitGet.h"

#include "pdt/PartitionNode.hpp"

using namespace dune;

// SetUHALLog is an attempt at a fiddle.  It is a class that does nothing except that it's
// constructor calls the uhal singleton to change the logging level to Notice.   The problem
// is that otherwise, when the constructor for the TimingReceiver is called, it creates the 
// connection manager before we have the opportunity to use the singleton, so the connection
// managers creation happens at full verbosity!   By listing the SetUHALLog logFiddle_ before
// the connectionManager_ in the class definition for TimingReceiver, we can make it call 
// in the right order.
dune::SetUHALLog::SetUHALLog(int) {
  uhal::setLogLevelTo(uhal::Notice());
}

// Constructor ------------------------------------------------------------------------------
dune::TimingReceiver::TimingReceiver(fhicl::ParameterSet const & ps):
  CommandableFragmentGenerator(ps)
  ,instance_name_("TimingReceiver")
  ,stopping_flag_(0)
  ,throttling_state_(0)
  ,stopping_state_(0)
  ,inhibitget_timer_(ps.get<uint32_t>("inhibit_get_timer",5000000))    // 3 secs TODO: Should make this a ps.get()
  ,logFiddle_(0)             // This is a fudge, see explanation above  
  ,connectionsFile_(ps.get<std::string>("connections_file", "/home/artdaq1/giles/a_vm_mbp/dune-artdaq/Generators/timingBoard/connections.xml"))
  ,bcmc_( "file://" + connectionsFile_ )   // a string (non-const)
  ,connectionManager_(bcmc_)
  ,hw_(connectionManager_.getDevice(ps.get<std::string>("hardware_select","DUNE_SECONDARY")))
  ,poisson_(ps.get<uint32_t>("poisson",0))
  ,divider_(ps.get<uint32_t>("divider",0xb))
  ,debugprint_(ps.get<uint32_t>("debug_print",0))
  ,initsoftness_(ps.get<uint32_t>("init_softness",0))
  ,startaltern_(ps.get<uint32_t>("start_altern",0))
  ,fw_version_active_(ps.get<uint32_t>("fw_version_active",3))
  ,fake_spill_cycle_(ps.get<uint32_t>("fake_spill_cycle",100))
  ,fake_spill_length_(ps.get<uint32_t>("fake_spill_length",50))
  ,main_trigger_enable_(ps.get<uint32_t>("main_trigger_enable",0))
  ,calib_trigger_enable_(ps.get<uint32_t>("calib_trigger_enable",1))
  ,trigger_mask_(ps.get<uint32_t>("trigger_mask",0xff))
  ,partition_(ps.get<uint32_t>("partition_",0))
  ,end_run_wait_(ps.get<uint32_t>("end_run_wait",1000))
  ,zmq_conn_(ps.get<std::string>("zmq_connection","tcp://pddaq-gen05-daq0:5566"))
{

    // TODO:
    // AT: Move hardware interface creation in here, for better exception handling

    int board_id = 0; //:GB  ps.get<int>("board_id", 0);
    std::stringstream instance_name_ss;
    instance_name_ss << instance_name_ << board_id;
    instance_name_ = instance_name_ss.str();

    if (inhibitget_timer_ == 0) inhibitget_timer_ = 2000000000;  // Zero inhibitget_timer waits infinite.

    //
    // Dave N's message part 1:
    // The pre-partition-setup status needs to be:
    //
    // - Board set up (clocks running and checked, etc)  [in hw_init, with initsoftness_=0]
    // - Timestamp set (if we ever do time-of-day stuff) [not done yet]
    // - Triggers from trigger enabled and gaps set      [not done yet]
    // - Command generators set up                       [done here] 
    // - Spills or fake spills enabled                   [wait for firmware upgrade]

    // Set up clock chip etc on board

    // PAR 2018-03-08. We're not doing any initialization of the board
    // itself any more: that's done in the butler
    // 
    // TimingSequence::hwinit(hw_,initsoftness_);

    if (debugprint_ > 1) {
      TimingSequence::hwstatus(hw_);
      TimingSequence::bufstatus(hw_);
    }

    // AT: Ensure that the hardware is up and running.
    // Check that the board is reachable
    // Read configuration version
    ValWord<uint32_t> lVersion = hw_.getNode("master.global.version").read();
    hw_.dispatch();

    // Match Fw version with configuration
    DAQLogger::LogInfo(instance_name_) << "Timing Master firmware version " << std::showbase << std::hex << (uint32_t)lVersion;
    if ( lVersion != fw_version_active_ ) {
      DAQLogger::LogWarning(instance_name_) << "Firmware version mismatch! Expected: " <<  fw_version_active_ << " detected: " << (uint32_t)lVersion;
    }

    // Measure the input clock frequency
    hw_.getNode("io.freq.ctrl.chan_sel").write(0);
    hw_.getNode("io.freq.ctrl.en_crap_mode").write(0);
    hw_.dispatch();
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    uhal::ValWord<uint32_t> fq = hw_.getNode("io.freq.freq.count").read();
    uhal::ValWord<uint32_t> fv = hw_.getNode("io.freq.freq.valid").read();
    hw_.dispatch();


    float lFrequency = ((uint32_t)fq) * 119.20928 / 1000000;
    DAQLogger::LogInfo(instance_name_) << "Measured timing master frequency: " << lFrequency << " Hz";

    if ( lFrequency < 240. || lFrequency > 260) {
      DAQLogger::LogError(instance_name_) << "Timing master clock out of expected range: " << lFrequency << " Hz. Has it been configured?";
    }

    // Probably the two following commands are to be removed

    // Command generators setup
    hw_.getNode("master.scmd_gen.ctrl.en").write(1); //# Enable sync command generators
    hw_.getNode("master.scmd_gen.chan_ctrl.en").write(0); //# Stop the command stream
    hw_.dispatch();

    // Dave N's message part 2:
    // So a likely cold-start procedure for the partition (not the board!) would be:
    //
    // - Disable the partition, just in case             [done here]
    // - Disable triggers                                [done here, repeated at start()]
    // - Disable buffer                                  [done here, repeated at start()]
    // - Enable the partition                            [done here, repeated at start()]
    // - Set the command mask                            [done here, repeated at start()]
    // - Enable triggers                                 [Not done until inhibit lifted (in get_next_())]


    // arguments to enable() are whether to enable and whether to
    // dispatch() respectively
    master_partition().enable(0, true);
    master_partition().stop();
    master_partition().enable(1, true);
    master_partition().writeTriggerMask(0x0f);


    // Set up connection to Inhibit Master
    InhibitGet_init(zmq_conn_.c_str(),inhibitget_timer_);

    DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";
}

// start() routine --------------------------------------------------------------------------
void dune::TimingReceiver::start(void)
{
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
    master_partition().writeTriggerMask(0x0f);
    master_partition().enable(1);

    // Dave N's message part 3
    // To start a run (assuming that no data has been read out between runs):
    //
    // - Disable buffer                                  [done by master_partition().start()]
    // - Enable buffer                                   [done by master_partition().start()]
    // - Set run_req                                     [done by master_partition().start()]
    // - Wait for run_stat to go high                    [done by master_partition().start()]


    try{
        master_partition().start();
    } catch(pdt::RunRequestTimeoutExpired& e){
        
    }

    // From pdtbutler::trigger function
    if (calib_trigger_enable_ != 0) {
      hw_.getNode("master.scmd_gen.chan_ctrl.type").write(3); //# Set command type = 3 for generator 0
      uint32_t rate = (divider_ > 0xf) ? 0xf : divider_;              // Allow 0x0 -> 0xf
      hw_.getNode("master.scmd_gen.chan_ctrl.rate_div").write(rate); //# Set about 1Hz rate for generator 0
      uint32_t pat = (poisson_ != 0) ? 1 : 0;
      hw_.getNode("master.scmd_gen.chan_ctrl.patt").write(pat); //# Set Poisson mode for generator 0
      hw_.dispatch();
      //echo( "> Setting trigger rate to {:.3e} Hz".format((50e6/(1<<(12+divider)))))
      //echo( "> Trigger spacing mode:" + {False: 'equally spaced', True: 'poisson'}[poisson] )

      hw_.getNode("master.scmd_gen.chan_ctrl.en").write(1); //# Start the command stream
      hw_.dispatch();
    }

    if (debugprint_ > 1) {
      TimingSequence::bufstatus(hw_);
    }
    InhibitGet_retime(inhibitget_timer_);

    // This is not the final enable.  That is done in getNext_() below. 
    // hw_.getNode("master.scmd_gen.chan_ctrl.en").write(1);
    // hw_.dispatch();

    this->reset_met_variables(false);
}


void dune::TimingReceiver::stop(void)
{
    DAQLogger::LogInfo(instance_name_) << "stop() called";

    stopping_flag_ = 1;   // We do want this here, if we don't use an 
                          // atomic<int> for stopping_flag_ (see header 
                          // file comments) 
}


void dune::TimingReceiver::stopNoMutex(void)
{
    DAQLogger::LogInfo(instance_name_) << "stopNoMutex called";

    stopping_flag_ = 1;    // Tells the getNext_() while loop to stop the run
}

//#define D(a) a
#define D(a)
#define E(a) a

bool dune::TimingReceiver::getNext_(artdaq::FragmentPtrs &frags) 
{
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
      // Stop the run, disable buffer and triggers
      master_partition().stop();
      // Disable the partition.
      //
      // PAR 2018-03-08 Quoth Dave on slack:
      //
      // """
      // I don’t think you ever want to disable the partition Stopping
      // the run should be enough If you stop the partition,
      // timestamps will stop gooing to the endpoints, and everything
      // will instantly go out of sync.
      // The basic ‘master control’ is run start / stop
      // """
      // 
      // So we won't disable the partition here
      // master_partition().enable(0, true);
      
    // Order is first (1) disable trig_en, (2) request run stop, (3) wait for run to stop, (4) check no more to read from buffer, (5) disable buff, (6) disable part

      hw_.dispatch();
      throttling_state_ = 0;  // Note for now, we remove XOFF immediately at end
                              // of run, discuss with hw people if that is right.
      stopping_state_ = 1;    // HW has been told to stop, may have more data
      usleep(end_run_wait_);  // Wait the max time spec for hardware to push more events 
                              // after a run has stopped
    }

    unsigned int havedata = (master_partition().numEventsInBuffer()>0);   // Wait for a complete event
    if (havedata) timeout = 1;  // If we have data, we don't wait around in case there is more or a throttle

    // TODO: Consider moving throttling check up here; as it is, we could receive a huge rate of events even though we
    // are also being told to throttle, and may not notice that throttling request.

    if (havedata) {   // An event or many events have arrived
      while (havedata) {

        // Make a fragment.  Follow the way it is done in the SSP boardreader
        std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( TimingFragment::size()*sizeof(uint32_t));

        // Read the data from the hardware
	std::vector<uint32_t> uwords = master_partition().readEvents(1);

#ifdef SEPARATE_DEBUG
        uint32_t word[6];  // Declare like this if we want a local variable instead of putting it straight in the fragmenti
#else
        uint32_t* word = reinterpret_cast<uint32_t *> (f->dataBeginBytes());    // dataBeginBytes returns a byte_t pointer
#endif
        for (int i=0;i<6;i++) {  word[i] = uwords[i]; }   // Unpacks from uHALs ValVector<> into the fragment

        dune::TimingFragment fo(*f);    // Overlay class - the internal bits of the class are
                                        // accessible here with the same functions we use offline.

        // Print out, can do it via the overlay class or with raw access
#ifdef SEPARATE_DEBUG
        // This way of printing also works in non-SEPARATE_DEBUG
        printf("Data: 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n"
                       ,word[0],word[1],word[2],word[3],word[4],word[5]);
#else
        if (debugprint_ > 2) {
          DAQLogger::LogDebug(instance_name_) << fo;
        }
#endif

        // Fill in the fragment header fields (not some other fragment generators may put these in the
        // constructor for the fragment, but here we push them in one at a time.
        f->setSequenceID( ev_counter() );  // ev_counter is in our base class  // or f->setSequenceID(fo.get_evtctr())
        f->setFragmentID( fragment_id() ); // fragment_id is in our base class, fhicl sets it
        f->setUserType( dune::detail::TIMING );
        //  No metadata in this block

        DAQLogger::LogInfo(instance_name_) << "For timing fragment with sequence ID " << ev_counter() << ", setting the timestamp to " << fo.get_tstamp();
        f->setTimestamp(fo.get_tstamp());  // 64-bit number

        frags.emplace_back(std::move(f));
        ev_counter_inc();
        havedata = false;    // Combined with the while above, we could make havedata an integer 
                             // and read multiple events at once (so use "havedata -= 6;")
        
        this->update_met_variables(fo);

        // TODO:  Update when we know what the spill start type is
        if (fo.get_scmd() == 0) this->reset_met_variables(true);  // true = only per-spill variables 

      }  // end while
    } else if (stopping_state_ > 0) { // We now know there is no more data at the stop
      stopping_state_ = 2;            // This causes return to be false (no more data)
      // TODO PAR: PartitionNode::stop() is presumably what we want,
      // but it will also disable triggers (trig_en -> 0). Not sure
      // what to do. But, Dave on slack, 2018-03-08:
      // """
      // Likewise, not clear why we’d ever want to run with buf_en
      // disabled, it’s really there so you can make sure the buffer
      // is clear at run start
      //
      // ie. toggle it low then high before starting a run
      // """
      // hw_.getNode("master.partition0.csr.ctrl.buf_en").write(0); //# Disable buffer in partition 0
      // hw_.dispatch();
      DAQLogger::LogInfo(instance_name_) << "Fragment generator returning clean run stop and end of data";
      break;
    } else {
       std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
       //usleep(timeout);   
    }

    // Check for throttling change
    // Do not allow a throttling change after the run has been requested to stop
    do {     // do {} while(false); allows pleasing use of break to get out without lots of nesting
      if (stopping_flag_ != 0) break;      // throttling change not desired after run stop request.      
      uint32_t tf = InhibitGet_get();      // Can give 0=No change, 1=OK, 2=Not OK)
      uint32_t bit = 1;                    // If we change, this is the value to set. 1=running 
      if (tf == 0) break;                  // No change, so no need to do anything
      if (debugprint_ > 2) {
        DAQLogger::LogDebug(instance_name_) << "Received value " << tf << " from InhibitGet_get()\n";
      }
      if (tf == 1) {                       // Want to be running
        if (throttling_state_ != 1) break; // No change needed
        bit = 1;                           // To set running (line not needed, bit is set to 1 already)
      } else if (tf == 2) {                // Want to be stopped
        if (throttling_state_ == 1) break; // Already stopped, no change needed
        bit = 0;                           // To set stopped
      } else {                             // Should never happen, InhibitGet_get returned unknown value.
        DAQLogger::LogWarning(instance_name_) << "TimingReceiver_generator.cc: Logic error should not happen";
        break;                             // Treat as no change
      }
      if (debugprint_ > 0) { 
        DAQLogger::LogInfo(instance_name_) << "Throttle state change: Writing " << bit 
                                           << " to trig_en.  [Throttling state was " << throttling_state_
                                           << "] (1 means enabled)\n";
      }
      // TODO PAR: PartitionNode::stop() will disable both the buffer
      // (buf_en -> 0) and the triggers (trig_en -> 0), so not sure
      // what to do here
      hw_.getNode("master.partition0.csr.ctrl.trig_en").write(bit);  // Set XOFF or XON as requested
      hw_.dispatch();
      throttling_state_ = bit ^ 0x1;       // throttling_state is the opposite of bit
    } while (false);                       // Do loop once only (mainly to have lots of 'break's above)

    // Limit the number of tests we do before returning 
    if (!(counter<max_counter)) break;
    ++counter;
  }

  this->send_met_variables();

  if (stopping_state_ == 2) return false;    // stopping_state = 2 means we know there is no more data 
  return true;   // Note: This routine can return (with a false) from inside loop (when 
                 // run has stopped and we know there is no more data)
}

bool dune::TimingReceiver::startOfDatataking() { return true; }

// Local "private" methods
const pdt::PartitionNode& dune::TimingReceiver::master_partition() {
    return hw_.getNode<pdt::PartitionNode>("master.partition0");
}

void dune::TimingReceiver::reset_met_variables(bool onlyspill) {

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

void dune::TimingReceiver::update_met_variables(dune::TimingFragment& fo) {

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
}

void dune::TimingReceiver::send_met_variables() {

  // When running in standalone mode, there's no metric manager, so just skip this bit
  if(!artdaq::Globals::metricMan_){
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
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TimingReceiver) 
