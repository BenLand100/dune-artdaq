#ifndef dune_artdaq_Generators_Candidate_hh
#define dune_artdaq_Generators_Candidate_hh

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
  

  class Candidate : public artdaq::CommandableFragmentGenerator {
  public:
    explicit Candidate(fhicl::ParameterSet const & ps);

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

    // Skip the retry timeout for now JLS June 2019
    // The amount of time the BR should wait before the next call to the TPReceiver (ms)
    //int waitretry_;

    // The maximum  number of time the BR should try to call the TPReceiver
    //size_t ntimes_retry_;

    bool stopping_flag_;

    // Socket sender
    std::vector<std::string> recvsocket_;
    // SocketReceiver
    std::vector<std::string> sendsocket_;

    // The TPSet receivers/senders
    ptmp::TPReceiver receiver_;
    ptmp::TPSender sender_;

    // TPwindows to window the TPSets, one for each Felix link
    std::vector<std::unique_ptr<ptmp::TPWindow>> tpwindows_;
                                                               
    // The TPwindow input/output IP and port connections
    std::vector<std::string> tpwinsocks_;
    std::vector<std::string> tpwoutsocks_;

    // TPwindow parameters
    // tspan - The width of the TP window
    // tbuf - The length of the buffer, in which the TPs are stored before being sent
    uint64_t tspan_;
    uint64_t tbuf_;

    // TPSorted serializes the data from the 10 links
    std::unique_ptr<ptmp::TPSorted> tpsorted_;

    // The inputs/output of TPsort
    std::vector<std::string> tpsortinsocks_;
    std::string tpsortout_;

    // The time TPsort will wait for a late TPSet
    int tardy_;

    // Counters
    size_t nTPset_recvd_;
    size_t nTPset_sent_;
    size_t nTPhits_;

    // Time counters
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    
    int n_recvd_;
    unsigned int p_count_;
    

  };
}

#endif /* dune_artdaq_Generators_Candidate_hh */
