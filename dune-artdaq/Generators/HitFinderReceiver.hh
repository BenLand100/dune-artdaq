#ifndef dune_artdaq_Generators_HitFinderReceiver_hh
#define dune_artdaq_Generators_HitFinderReceiver_hh

//################################################################################
//# /*
//#      HitFinderReceiver.hh (goes with: HitFinderReceiver_generator.cc)
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


using namespace uhal;

namespace dune {
  class HitFinderReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit HitFinderReceiver(fhicl::ParameterSet const & ps);

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

  };
}

#endif /* dune_artdaq_Generators_HitFinderReceiver_hh */
