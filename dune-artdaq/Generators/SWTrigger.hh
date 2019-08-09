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
#include "dune-artdaq/Generators/Felix/ProducerConsumerQueue.hh"

#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"
#include "ptmp-tcs/api.h"

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
    
    void tpsetHandler();

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

    // This gets set in the main thread by stop() and read in the
    // tpsethandler thread, so we make it atomic just in case
    std::atomic<bool> stopping_flag_;

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

    // TPset receving and sending thread
    std::thread tpset_handler;

    // TPZipper serializes the TPSets from the APAs
    std::vector<std::string> tc_inputs_;
    std::unique_ptr<ptmp::TPZipper> tczipper_;

    // Interface to TD algorithm
    std::unique_ptr<ptmp::tcs::TPFilter> tdGen_;

    ptmp::TPSender sender_;

    folly::ProducerConsumerQueue<ptmp::data::TPSet> queue_{100000};
    ptmp::data::TPSet* tpset_;

    // TPZipper serializes the data from multiple links
    std::unique_ptr<ptmp::TPZipper> tpzipper_;

    // Interface to TC algorithm
    std::unique_ptr<ptmp::tcs::TPFilter> tcGen_;

    // The maximum time in microseconds before we timeout for a TPReceiver call (ms)
    int timeout_;

    int n_recvd_;
    // How many trigger candidate inputs we're listening to
    size_t n_inputs_;
    
    // Use TC prescale for trigger
    bool faketrigger_;
    // TC prescale
    int prescale_;

    // Output of TPZipper for TC inputs
    std::string tczipout_;
    // TPZipper link tardy time
    int tardy_;

    // Output of TD algorithm
    std::string tdout_;

    // The Module Level algorithm
    std::string td_alg_;

    // Per-input counts:
    size_t prev_counts_; // The value of TPSet::count() for TPZipper output in the previous go-round
    size_t norecvds_;    // How many times the input has timed out
    size_t n_recvds_;    // How many TPSets have been received on the input
    size_t nTPhits_;     // How many TrigPrims have been received on the input
    size_t ntriggers_;
    size_t fqueue_;
    size_t loops_;
    size_t qtpsets_;
    
    size_t count_;

  };
}

#endif /* dune_artdaq_Generators_SWTrigger_hh */
