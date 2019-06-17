#ifndef dune_artdaq_Generators_SWTriggerReceiver_hh
#define dune_artdaq_Generators_SWTriggerReceiver_hh

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"


namespace dune {
  class SetZMQSigHandler {
    public:
      SetZMQSigHandler();   // Constructor
  };
  

  class SWTriggerReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit SWTriggerReceiver(fhicl::ParameterSet const & ps);

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator
    bool getNext_(artdaq::FragmentPtrs & output) override;

    // Wes, 18 July 2018
    // Use a checkHWStatus_ function to run a thread on the side to
    // check buffers and issue inhibits.
    // Pierre: for now this returns true all thetime since we don't yet care about the inhibit master
    bool checkHWStatus_() override;

    // JCF, Dec-11-2015
// startOfDatataking will determine whether or not we've begun
    // taking data, either because of a new run (from the start
    // transition) or a new subrun (from the resume transition). It's
    // designed to be used to determine whether no fragments are
    // getting sent downstream (likely because of upstream hardware
    // issues)
    // Pierre: Not too sure what this is supposed to do
    bool startOfDatataking();

    // State transition methods, for future use, if/when needed
    void start() override; // {}
    void stop() override; // {}
    void stopNoMutex() override;
    void pause() override {}
    void resume() override {}

    // Reporting functionality, for future use, if/when needed
    std::string report() override { return ""; }
    
    dune::SetZMQSigHandler zmq_sig_handler_;  // See explanation of this fiddle in 

    // An name for what we want to log to.
    std::string instance_name_;

    // The maximum time in microseconds before we timeout for a TPReceiver call (ms)
    int timeout_;

    // The amount of time the BR should wait before the next call to the TPReceiver (ms)
    int waitretry_;

    // The maximum  number of time the BR should try to call the TPReceiver
    size_t ntimes_retry_;
    
    // The number of input PTMP messages in the outpout
    size_t aggregation_;


    bool must_stop_;

    ptmp::TPSorted tpsorted_;
    ptmp::TPReceiver receiver_;

      int nrecvd_;
  };
}

#endif /* dune_artdaq_Generators_SWTriggerReceiver_hh */
