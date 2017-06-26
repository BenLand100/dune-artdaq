//################################################################################
//# /*
//#        TimingReceiver_generato.cpp
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
  ,throttling_flag_(0)
  ,stopping_flag_(0)
  ,throttling_state_(0)
  ,stopping_state_(0)
  ,logFiddle_(0)             // This is a fudge, see explanation above  
  ,bcmc_( "file://connections.xml" )   // a string (non-const)
  ,connectionManager_(bcmc_)
  ,hw_(connectionManager_.getDevice("DUNE_FMC_RX"))
{
    int board_id = 0; //:GB  ps.get<int>("board_id", 0);
    std::stringstream instance_name_ss;
    instance_name_ss << instance_name_ << board_id;
    instance_name_ = instance_name_ss.str();

    instance_name_for_metrics_ = "";

    // Set up clock chip etc on board
    TimingSequence::hwinit(hw_,"SI5344/PDTS0003.txt");
    TimingSequence::hwstatus(hw_);
    TimingSequence::bufstatus(hw_);

    DAQLogger::LogInfo(instance_name_) << "Done configure (end of constructor)\n";
}

// start() routine --------------------------------------------------------------------------
void dune::TimingReceiver::start(void)
{
    DAQLogger::LogDebug(instance_name_) << "start() called\n";

    // See header file for meanings of these variables
    stopping_flag_    = 0;
    stopping_state_   = 0;
    throttling_flag_  = 0;  // Equiv to throttling_flag_.store(0,memory_order_seq_cst);
    throttling_state_ = 0;

    hw_.getNode("master.partition.csr.ctrl.part_en").write(1);

    hw_.getNode("master.scmd_gen.ctrl.en").write(1);
    hw_.getNode("master.partition.csr.ctrl.cmd_mask").write(0x000f);
    hw_.getNode("master.partition.csr.ctrl.trig_en").write(1);
    hw_.getNode("master.scmd_gen.chan_ctrl.type").write(3);
    hw_.getNode("master.scmd_gen.chan_ctrl.rate_div").write(0xb);
    hw_.getNode("master.scmd_gen.chan_ctrl.patt").write(1);
    hw_.getNode("master.partition.csr.ctrl.buf_en").write(1);
    // hw.getNode("endpoint.csr.ctrl.buf_en").write(1)
    hw_.dispatch();

    TimingSequence::bufstatus(hw_);

    // TODO: This is the final enable.  It should not be done here 
    // because there could be a delay before GetNext() is called for 
    // first time (is that true?)  So we need logic in GetNext to 
    // find the first time it is called after a start().
    hw_.getNode("master.scmd_gen.chan_ctrl.en").write(1);
    hw_.dispatch();
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
  int max_timeout = 50;  // TODO: This should be in nsecs

  // Technique: Use a generic loop (i.e. exit is from break or return statements, 
  // not from a while (condition).  This allows us to easily reorder the sequence 
  // of actions to get them right
  while (true) {
    int timeout = max_timeout;  // If no event found we wait this time

    // Check for stop run
    if (stopping_state_ == 0 && stopping_flag_ != 0) {
      //----hwx_.getNode("partition.csr.ctrl.part_en").write(0); // Disable the run first (stops)
      //----hwx_.getNode("partition.csr.ctrl.trig_en").write(0); // Now unset the trigger_enable (XOFF)
      //----hwx_.dispatch();
      throttling_state_ = 0;  // Note for now, we remove XOFF immediately at end
                              // of run, discuss with hw people if that is right.
      stopping_state_ = 1;    // HW has been told to stop, may have more data
      usleep(10);             // Wait the max time spec for hardware to push more events 
                              // after a run has stopped
    }

    // Check for new event data
    ValWord<uint32_t> valr = hw_.getNode("master.partition.buf.count").read();  // Can be max 65536 
          // TODO: Make "master.partition" a parameter so we can change partition, or use on an endpoint
    hw_.dispatch();
    std::cout << "valr " << valr << std::endl;
    unsigned int havedata = (valr.value() > 5);   // Wait for a complete event - 6 words TODO: Not hardwired length
    if (havedata) timeout = 1;  // If we have data, we don't wait around in case there is more or a throttle

    // TODO: Consider moving throttling check up here; as it is, we could receive a huge rate of events even though we
    // are also being told to throttle, and may not notice that throttling request.

    if (havedata) {   // An event or many events have arrived
      while (havedata) {

        // Make a fragment.  Follow the way it is done in the SSP boardreader
        std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( TimingFragment::size()*sizeof(uint32_t));

        // Read the data from the hardware
        ValVector<uint32_t> uwords = hw_.getNode("master.partition.buf.data").readBlock(6);  
        hw_.dispatch();

        // uint32_t word[6];  // Declare like this if we want a local variable instead of putting it straight in the fragmenti
        uint32_t* word = reinterpret_cast<uint32_t *> (f->dataBeginBytes());    // dataBeginBytes returns a byte_t pointer
        for (int i=0;i<6;i++) {  word[i] = uwords[i]; }   // Unpacks from uHALs ValVector<> into the fragment

        dune::TimingFragment fo(*f);    // Overlay class - the internal bits of the class are
                                        // accessible here with the same functions we use offline.

        // Print out, can do it via the overlay class or with raw access
        std::cout << fo;
        // printf("Data: 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x 0x%8.8x\n"
        //                ,word[0],word[1],word[2],word[3],word[4],word[5]);

        // Fill in the fragment header fields (not some other fragment generators may put these in the
        // constructor for the fragment, but here we push them in one at a time.
        f->setSequenceID( ev_counter() );  // ev_counter is in our base class  // or f->setSequenceID(fo.get_evtctr())
        f->setFragmentID( fragment_id() ); // fragment_id is in our base class, fhicl sets it
        f->setUserType( dune::detail::TIMING );
        //  No metadata in this block
        f->setTimestamp(fo.get_tstamp());  // 64-bit number

        frags.emplace_back(std::move(f));
        ev_counter_inc();
        havedata = false;    // Combined with the while above, we could make havedata an integer 
                             // and read multiple events at once (so use "havedata -= 6;")
      }  // end while
    } else if (stopping_state_ > 0) { // We now know there is no more data at the stop
      stopping_state_ = 2;            // This causes return to be false (no more data)
      break;
    } else {
       std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
       //usleep(timeout);   
       // TODO: If we use Tim's SafeQueue() trick we can incorporate this wait into the throttling_flag_ change
    }

    // Check for throttling change over a small period of time (throttling_flag_ is set by separate thread)
    // Do not allow a throttling change after the run has been requested to stop
    // TODO: Replace this by code copied from Tim's SafeQueue() condition_variable approach. 
    //    (1) It will wake up immediately if the other thread changes, 
    //    (2) it is better than the atomic<int> approach for insuring the optimiser doesn't 
    //        lift it out the loop and 
    //    (3) better than the usleep() in the loop.

    if (stopping_flag_ == 0) {       // throttling change not allowed after run stop request.
      int tf = throttling_flag_;     // tf = throttling_flag_.load(memory_order_seq_cst); 
      if (tf != throttling_state_) { // Is XOFF requested different to XOFF in hardware?
	// Commented out to get it compiling 2017/06/23
        //int bit = (tf==0) ? 0 : 1;   // This is probably unnecesary as tf should only be 0 or 1, in whcih case bit = tf.
        //----hwx_.getNode("partition.csr.ctrl.trig_en").write(bit);  // Set XOFF or XON as requested
        //----hwx_.dispatch();
      }
      throttling_state_ = throttling_flag_;  // TODO: Should we read back to check, and can hit again 
                                             // next time round loop if bad. 
    }

    // Limit the number of tests we do before returning 
    if (!(counter<max_counter)) break;
    ++counter;
  }

  if (stopping_state_ == 2) return false;    // stopping_state = 2 means we know there is no more data 
  return true;   // Note: This routine can return (with a false) from inside loop (when 
                 // run has stopped and we know there is no more data)
}

bool dune::TimingReceiver::startOfDatataking() { return true; }

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TimingReceiver) 
