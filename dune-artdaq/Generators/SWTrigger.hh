#ifndef dune_artdaq_Generators_SWTrigger_hh
#define dune_artdaq_Generators_SWTrigger_hh

//################################################################################
//# /*
//#      SWTrigger.hh (goes with: SWTrigger_generator.cc)
//#
//#  GLM March 2019 
//#  Inspired from TimingReceiver
//# */
//################################################################################

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"

#include "timingBoard/StatusPublisher.hh"
#include "timingBoard/FragmentPublisher.hh"
#include "timingBoard/HwClockSubscriber.hh"


namespace dune {
  class TimingFragment;
}

namespace dune {
  class SetZMQSigHandler {
  public:
    SetZMQSigHandler(); //Constructor
  };

  class SWTrigger : public artdaq::CommandableFragmentGenerator {
  public:
    explicit SWTrigger(fhicl::ParameterSet const & ps);

  protected:
    std::string metricsReportingInstanceName() const {
      return "";
    }

  private:

    void readTS();
    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    bool checkHWStatus_() override;

    // State transition methods, for future use, if/when needed
    void start() override; // {}
    void stop() override; // {}
    void stopNoMutex() override;
    void pause() override {}
    void resume() override {}

    // Reporting functionality, for future use, if/when needed
    std::string report() override { return ""; }

    dune::SetZMQSigHandler zmq_sig_handler_;

    std::string instance_name_;

    bool stopping_flag_;

    bool throttling_state_;

    uint32_t inhibitget_timer_; // Time in us before we give up on the InhibitGet and
                                // just start the run.  A value over 10000000 (10s) 
                                // is treated as infinite time wait (which should
                                // be used for production running)
    

    uint32_t partition_number_; // The partition number we're talking to
    uint32_t debugprint_;  // Controls the printing of stuff as info messages. 
                           // 0=minimal, 
                           // 1=Also reports throttling changes that go to hardware, 
                           // 2=calls the bufstatus and hwstatus during init, 
                           // 3=Debug message for each trigger and all throttling data

    std::string zmq_conn_;  // String specifying the zmq connection to subscribe for inhibit information

    std::unique_ptr<artdaq::StatusPublisher> status_publisher_;
    std::unique_ptr<artdaq::FragmentPublisher> fragment_publisher_;
    std::unique_ptr<HwClockSubscriber> ts_receiver_;
    std::unique_ptr<std::thread> ts_subscriber_;

    bool want_inhibit_; // Do we want to request a trigger inhibit?

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
    std::atomic<uint64_t> latest_ts_; // needs to be atomic as shared between threads
    uint64_t previous_ts_; // only used within getNext

    ptmp::TPReceiver receiver_1_;
    ptmp::TPReceiver receiver_2_;

    int n_recvd_;

    unsigned int p_count_1_;
    unsigned int p_count_2_;

  };
}

#endif /* dune_artdaq_Generators_SWTrigger_hh */
