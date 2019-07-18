#ifndef dune_artdaq_Generators_TimingReceiver_hh
#define dune_artdaq_Generators_TimingReceiver_hh

//################################################################################
//# /*
//#      TimingReceiver.hh (goes with: TimingReceiver_generator.cpp)
//#
//#  Giles Barr, Justo Martin-Albo, Farrukh Azfar, Jan 2017,  May 2017 
//#  for ProtoDUNE
//# */
//################################################################################

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Generators/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push
#include "uhal/uhal.hpp"   // The real uhal
#pragma GCC diagnostic pop
#include "timingBoard/StatusPublisher.hh"
#include "timingBoard/FragmentPublisher.hh"

//class testclass : private boost::noncopyable {
//public:
//  testclass(std::string& initstring);
//};

// This little class is a fiddle, the explanation is in the top of 
// TimingReceiver_generator.cc.   It is to avoid the uhal verbose messages 
// as the connectionManager is being constructed
namespace dune {
  class SetUHALLog {
    public:
      SetUHALLog(int);   // Constructor
  };
}

using namespace uhal;

namespace dune {
class TimingFragment;
}

namespace pdt {
    class PartitionNode; // Forward definition
}

namespace dune {
  class TimingReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TimingReceiver(fhicl::ParameterSet const & ps);

  protected:
    std::string metricsReportingInstanceName() const {
      return "";
    }

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    // Wes, 18 July 2018
    // Use a checkHWStatus_ function to run a thread on the side to
    // check buffers and issue inhibits.
    bool checkHWStatus_() override;


    // JCF, Dec-11-2015

    // startOfDatataking will determine whether or not we've begun
    // taking data, either because of a new run (from the start
    // transition) or a new subrun (from the resume transition). It's
    // designed to be used to determine whether no fragments are
    // getting sent downstream (likely because of upstream hardware
    // issues)

    bool startOfDatataking();


    // State transition methods, for future use, if/when needed
    void start() override; // {}
    void stop() override; // {}
    void stopNoMutex() override;
    void pause() override {}
    void resume() override {}

    // Reporting functionality, for future use, if/when needed
    std::string report() override { return ""; }

    std::string instance_name_;

    // stopping_flag_ : This is written in stopNoMutex() and
    //     again in stop().  It is read inside the getNext_() while
    //     loop, and so atomic<int> may help it see the change in
    //     value from stopNoMutex(), by telling the compiler not to
    //     lift reading it to outside the while loop.  However,
    //     since stop() and GetNext() are on the same thread, it is
    //     probably faster to just let that happen (i.e. doing anything
    //     fancy with this variable is likely at least as slow as just
    //     waiting for stop() instead of stopNoMutex()).
    uint32_t stopping_flag_;

    uint32_t throttling_state_;  // 0=XON, 1=XOFF state as set in the hardware.
    uint32_t stopping_state_;    // 0=running, 1=told hardware to stop, but it 
                                 // may have more data for me, 2=hardware has
                                 // no more data, OK for getNext_() to return false

    // Items that are mostly FHICL parameters being poked down to the hardware
    // or other software components
    uint32_t inhibitget_timer_; // Time in us before we give up on the InhibitGet and
                                // just start the run.  A value over 10000000 (10s) 
                                // is treated as infinite time wait (which should
                                // be used for production running)
    
    // Items needed for uHAL
    dune::SetUHALLog logFiddle_;  // See explanation of this fiddle in 
             // TimingReceiver_generator.cc comments near the constructor.  
    const std::string connectionsFile_;
    std::string bcmc_;  // This is a fiddle because Connection Manager wants it non-Const  
    uhal::ConnectionManager connectionManager_; 
    uhal::HwInterface hw_;

    // Things that are Fhicl parameters
    uint32_t partition_number_; // The partition number we're talking to
    uint32_t debugprint_;  // Controls the printing of stuff as info messages. 
                           // 0=minimal, 
                           // 1=Also reports throttling changes that go to hardware, 
                           // 2=calls the bufstatus and hwstatus during init, 
                           // 3=Debug message for each trigger and all throttling data
    uint32_t trigger_mask_;         // Trigger mask
    uint32_t end_run_wait_;         // Number of microsecs to wait at the end of a run before looking for last event
    bool enable_spill_commands_; // Should we tell the board to enable spill start/stop as event-generating commands?
    bool enable_spill_gate_; // Whether to enable the spill gate on the timing board

    std::string zmq_conn_;  // String specifying the zmq connection to subscribe for inhibit information
    std::string zmq_conn_out_;  // String specifying the zmq connection we will send our inhibit information to
    std::string zmq_fragment_conn_out_; // String specifying the zmq connection we publish fragments on
    std::vector<int> valid_firmware_versions_fcl_; // Valid versions of the firmware according to the fcl file. We take the union of these and any versions hardcoded into the board reader as being allowed

    // Things for metrics (need to use int because the metrics send class signature uses 'int')
    int met_event_;    // Current event
    uint64_t met_tstamp_;   // Current timestamp (in 64-bit, may need to truncate to 32 bits to post to metrics
    int met_sevent_;   // Number of events this spill
    int met_rintmin_;  // Min interval between events this run
    int met_sintmin_;  // Min interval between events this spill
    int met_rintmax_;  // Max interval between events this run
    int met_sintmax_;  // Max interval between events this spill

    std::string trigger_outfile_dir_;
    std::vector<uint32_t> met_accepted_trig_count_;
    std::vector<uint32_t> met_rejected_trig_count_;

    std::unique_ptr<artdaq::StatusPublisher> status_publisher_;
    std::unique_ptr<artdaq::FragmentPublisher> fragment_publisher_;

    int want_inhibit_; // Do we want to request a trigger inhibit?

    // Some timestamps of the most recent of each type of event. We
    // fill these by reading events from the event buffer, and add
    // their values to the subsequent event fragments, so that the
    // information is in the data stream
    uint32_t last_spillstart_tstampl_; // Timestamp of most recent start-of-spill (low 32 bits)
    uint32_t last_spillstart_tstamph_; //                                         (high 32 bits)
    uint32_t last_spillend_tstampl_;   // Timestamp of most recent end-of-spill (low 32 bits)
    uint32_t last_spillend_tstamph_;   //                                       (high 32 bits)
    uint32_t last_runstart_tstampl_;   // Timestamp of most recent start-of-run (low 32 bits)
    uint32_t last_runstart_tstamph_;   //                                       (high 32 bits)

// Internal functions
    void reset_met_variables(bool onlyspill=false);   // If onlyspill, only reset the in-spill variables
    void update_met_variables(dune::TimingFragment& fo); // Update variables for met
    void send_met_variables();    // Send the variables to the metrics system
    void fiddle_trigger_mask();  // Modify the trigger mask so we only see triggers in our partition
    // TODO: Should this be const?
    const pdt::PartitionNode& master_partition();

    bool use_routing_master_;
  };
}

#endif /* dune_artdaq_Generators_TimingReceiver_hh */
