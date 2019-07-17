#ifndef dune_artdaq_Generators_Candidate_hh
#define dune_artdaq_Generators_Candidate_hh

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-artdaq/Generators/Felix/ProducerConsumerQueue.hh"
#include "swTrigger/AdjacencyAlgorithms.h"
#include "swTrigger/TriggerCandidate.h"

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

    // The tpsetHandler function is used to process the TPSets 
    // and associated TPs. This means it is resonsible for receiving
    // TPSets, via zeromq, passing them to TC algorithms and sending the resulting
    // TCs onnward, via zeromq.
    void tpsetHandler();

     // Routine to re-order the TPs by channel and sub-orer by time
     std::vector<TP> TPChannelSort(std::vector<ptmp::data::TPSet> HitSets); 

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

    // TPset receving and sending thread
    std::thread tpset_handler;

    // The TPSet receivers/senders
    //ptmp::TPReceiver receiver_;
    //ptmp::TPSender sender_;

    //std::unique_ptr<folly::ProducerConsumerQueue<ptmp::data::TPSet>> tpsetToFrag;
    folly::ProducerConsumerQueue<ptmp::data::TPSet> queue_{100000};
    ptmp::data::TPSet* tpset_;
 
    // TPwindows to window the TPSets, one for each Felix link
    std::unique_ptr<ptmp::TPWindow> tpwindow_01_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_02_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_03_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_04_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_05_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_06_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_07_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_08_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_09_;
    std::unique_ptr<ptmp::TPWindow> tpwindow_10_;
                                                               
    // The TPwindow input/output IP and port connections
    std::string tpwinsock_;
    // int tpwin_port_;
    std::string tpwoutsock_;
    // int tpwout_port_;

    // TPwindow parameters
    // tspan - The width of the TP window
    // tbuf - The length of the buffer, in which the TPs are stored before being sent
    uint64_t tspan_;
    uint64_t tbuf_;

    // TPZipper serializes the data from the 10 links
    std::unique_ptr<ptmp::TPZipper> tpzipper_;

    // The inputs/output of TPsort
    std::string tpzipinsock_;
    // int tpsortin_port_;
    std::string tpzipout_;

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
    unsigned int norecvd_;
    unsigned int stale_set_;
    unsigned int loops_;
    unsigned int fqueue_;
    unsigned int qtpsets_;
    unsigned int p_count_;
    

  };
}

#endif /* dune_artdaq_Generators_Candidate_hh */
