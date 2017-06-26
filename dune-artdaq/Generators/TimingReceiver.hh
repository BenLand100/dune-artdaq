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
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/TimingFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

#include "uhal/uhal.hpp"   // The real uhal


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

  class TimingReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TimingReceiver(fhicl::ParameterSet const & ps);

  protected:
    std::string metricsReportingInstanceName() const {
      return instance_name_for_metrics_;
    }

  private:

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
    std::string instance_name_for_metrics_;

    // The following two variables allow inter-thread communication
    // (1) throttling_flag_ : This is written only in the throttling 
    //     thread to indicate a desire for XOFF or XON again
    //     It is read in the getNext_() loop.  We should consider
    //     declaing it either "int" or "atomic<int>".  The downside
    //     with "int" is that I think it is possible for the read in
    //     getNext_() to be optimised outside the while loop, which
    //     would mean getNext_() would only see the change on it's
    //     next call. (There could be a bigger problem, that the
    //     next call also sees a cached copy).
    std::atomic<int> throttling_flag_;
    // ** STILL DON'T THINK atomic<> IS QUITE RIGHT, PROBABLY 
    // WILL GO WITH A CONDITION VARIABLE APPROACH IN NEXT REWRITE
    // ** atomic<> only guarantees the ordering of reads, but it can
    // still sit in a cache for ages. volatile is also not the solution

    // (2) stopping_flag_ : This is written in stopNoMutex() and
    //     again in stop().  It is read inside the getNext_() while
    //     loop, and so atomic<int> may help it see the change in
    //     value from stopNoMutex(), by telling the compiler not to
    //     lift reading it to outside the while loop.  However,
    //     since stop() and GetNext() are on the same thread, it is
    //     probably faster to just let that happen (i.e. doing anything
    //     fancy with this variable is likely at least as slow as just
    //     waiting for stop() instead of stopNoMutex()).
    int stopping_flag_;

    // Note, we use the 'standard form' of reading and assignment for
    // throttling_flag_ so we can try switching between using an 'int'
    // or an 'atomic<int>'. The atomic int reads and writes can also be
    // done with x.save(value) and value=x.load() which makes them look
    // more special :)

    // The following two variables give what the getNext_() while loop
    // was doing about each of the two actions.  They are only used in
    // the main reader thread, so don't need to be special
    int throttling_state_;  // XOFF/XON state as set in the hardware.
    int stopping_state_;  // 0=running, 1=told hardware to stop, but it 
                          // may have more data for me, 2=hardware has
                          // no more data, OK for getNext_() to return false
    
    // Items needed for uHAL
    dune::SetUHALLog logFiddle_;  // See explanation of this fiddle in 
             // TimingReceiver_generator.cc comments near the constructor.  
    std::string bcmc_;  // This is a fiddle because Connection Manager wants it non-Const  
    uhal::ConnectionManager connectionManager_; 
    uhal::HwInterface hw_;
  };
}

#endif /* dune_artdaq_Generators_TimingReceiver_hh */
