#ifndef dune_artdaq_Generators_HitFinderCPUReceiver_hh
#define dune_artdaq_Generators_HitFinderCPUReceiver_hh

//################################################################################
//# /*
//#      HitFinderCPUReceiver.hh (goes with: HitFinderCPUReceiver_generator.cc)
//#
//# PLasorak Feb 2019
//#  for ProtoDUNE
//# */
//################################################################################

// Some C++ conventions used:
// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "PTMPConfig.h"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "ptmp/api.h"

#pragma GCC diagnostic ignored "-Wunused"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic push
#include "uhal/uhal.hpp"   // The real uhal
#pragma GCC diagnostic pop


using namespace uhal;

namespace dune {

  class HitFinderCPUReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit HitFinderCPUReceiver(fhicl::ParameterSet const & ps);

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

    // An name for what we want to log to.
    std::string instance_name_;

    // The maximum time in microseconds before we timeout and say that no data is here
    int timeout_;
    
    // The 2 PTMP configurations
    ReceiverPTMP_Config receiver_config_;

    // The actual receiver/sender
    ptmp::TPReceiver receiver_;
    ptmp::TPSender sender_;

    size_t aggregation_;

    // Request receiver things. That should listen to PTMP and check whether there are hits broadcasted or not.
    // Not sure whether artdaq allows to do that.

    // std::unique_ptr<RequestReceiver> request_receiver_;
    // std::string requester_address_;
    // std::string request_address_;
    // unsigned short request_port_;
    // unsigned short requests_size_;
    
  };
}

#endif /* dune_artdaq_Generators_HitFinderCPUReceiver_hh */
