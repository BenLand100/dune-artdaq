#ifndef dune_artdaq_Generators_IsoMuonFinder_hh
#define dune_artdaq_Generators_IsoMuonFinder_hh

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-artdaq/Generators/Felix/ProducerConsumerQueue.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"


namespace dune {

    class IsoMuonFinder : public artdaq::CommandableFragmentGenerator {
    public:
        explicit IsoMuonFinder(fhicl::ParameterSet const & ps);

    private:

        virtual ~IsoMuonFinder();

        // The tpsetHandler function is used to process the TPSets 
        // and associated TPs. This means it is resonsible for receiving
        // TPSets, via zeromq, passing them to TC algorithms and sending the resulting
        // TCs onnward, via zeromq.
        void tpsetHandler();

        // The "getNext_" function is used to implement user-specific
        // functionality; it's a mandatory override of the pure virtual
        // getNext_ function declared in CommandableFragmentGenerator
        bool getNext_(artdaq::FragmentPtrs & output) override;

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

        folly::ProducerConsumerQueue<uint64_t> timestamp_queue_{10000};
                                                               
        // The TPwindow input/output IP and port connections
        std::vector<std::string> tpwinsocks_;
        std::vector<std::string> tpwoutsocks_;

        std::vector<std::unique_ptr<ptmp::TPWindow>> tpwindows_;
        // TPwindow parameters
        // tspan - The width of the TP window
        // tbuf - The length of the buffer, in which the TPs are stored before being sent
        uint64_t tspan_;
        uint64_t tbuf_;

        // TPZipper serializes the data from the 10 links
        std::unique_ptr<ptmp::TPZipper> tpzipper_;

        // The inputs/output of TPsort
        std::vector<std::string> tpzipinsocks_;
        std::string tpzipout_;

        // The time TPsort will wait for a late TPSet
        int tardy_;

        // Where we send the trigger message to the FELIX BRs
        std::string sendsocket_;

        // TPset receving and sending thread
        std::thread tpset_handler;

        size_t hit_per_link_threshold_;
    };
}

#endif /* dune_artdaq_Generators_IsoMuonFinder_hh */
