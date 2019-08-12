#ifndef dune_artdaq_Generators_Candidate_hh
#define dune_artdaq_Generators_Candidate_hh

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Generators/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"
//#include "swTrigger/AdjacencyAlgorithms.h"
//#include "swTrigger/TriggerCandidate.h"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"
#include "ptmp-tcs/api.h"

namespace dune {

  class Candidate : public artdaq::CommandableFragmentGenerator {
  public:
    explicit Candidate(fhicl::ParameterSet const & ps);

  private:

      virtual ~Candidate();

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
    
    // An name for what we want to log to.
    std::string instance_name_;

    // The maximum time in microseconds before we timeout for a TPReceiver call (ms)
    int timeout_;

    // This gets set in the main thread by stop() and read in the
    // tpsethandler thread, so we make it atomic just in case
    std::atomic<bool> stopping_flag_;

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

    // TPZipper serializes the data from the 10 links
    std::unique_ptr<ptmp::TPZipper> tpzipper_;

    // Interface to TC algorithm
    std::unique_ptr<ptmp::tcs::TPFilter> tcGen_;

    // The inputs/output of TPsort
    std::vector<std::string> tpzipinsocks_;
    std::string tpzipout_;

    // The time TPsort will wait for a late TPSet
    int tardy_;

    std::string recvsocket_;
    std::string sendsocket_;

    // The TC algorithm used in TPFilter
    std::string tc_alg_;


    // Counters
    size_t nTPset_recvd_;

    // Time counters
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    
    size_t n_recvd_;
    size_t loops_;
    

  };
}

#endif /* dune_artdaq_Generators_Candidate_hh */
