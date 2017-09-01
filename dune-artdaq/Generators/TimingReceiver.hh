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

    // stopping_flag_ : This is written in stopNoMutex() and
    //     again in stop().  It is read inside the getNext_() while
    //     loop, and so atomic<int> may help it see the change in
    //     value from stopNoMutex(), by telling the compiler not to
    //     lift reading it to outside the while loop.  However,
    //     since stop() and GetNext() are on the same thread, it is
    //     probably faster to just let that happen (i.e. doing anything
    //     fancy with this variable is likely at least as slow as just
    //     waiting for stop() instead of stopNoMutex()).
    uint32_t stopping_flag_;

    uint32_t throttling_state_;  // 0=XON, 1=XOFF state as set in the hardware.
    uint32_t stopping_state_;    // 0=running, 1=told hardware to stop, but it 
                                 // may have more data for me, 2=hardware has
                                 // no more data, OK for getNext_() to return false

    // Items that are mostly FHICL parameters being poked down to the hardware
    // or other software components
    uint32_t inhibitget_timer_; // Time in us before we give up on the InhibitGet and
                                // just start the run.  A value over 10000000 (10s) 
                                // is treated as infinite time wait (which should
                                // be used for production running)
    
    // Items needed for uHAL
    dune::SetUHALLog logFiddle_;  // See explanation of this fiddle in 
             // TimingReceiver_generator.cc comments near the constructor.  
    const std::string connectionsFile_;
    std::string bcmc_;  // This is a fiddle because Connection Manager wants it non-Const  
    uhal::ConnectionManager connectionManager_; 
    uhal::HwInterface hw_;

    uint32_t poisson_;  // 0=Set fixed rate, 1= Poission distributed
    uint32_t divider_;  // The divider to get the rate = 50MHz/2**(12+divider_)   [e.g. 0xb=5.9Hz]
    uint32_t debugprint_;  // Controls the printing of stuff as info messages. 
                           // 0=minimal, 
                           // 1=Also reports throttling changes that go to hardware, 
                           // 2=calls the bufstatus and hwstatus during init, 
                           // 3=Debug message for each trigger and all throttling data
    uint32_t initsoftness_; // Controls whether some of the initialisation sequence is bypassed to
                            // workaround inability of other systems (e.g. WIB) to recover from a
                            // clock reset quickly.
                            // 0 = full reset, 3 =  just read a few things, 4 = bypass initialisation completely
    uint32_t fw_version_active_;   // Default 3 for v3c, Activiate v5e features by setting to 5
    uint32_t fake_spill_length_;   // If zero, means no fake spills
    uint32_t fake_spill_cycle_;    // If zero, means no fake spills
    uint32_t main_trigger_enable_; // Enable triggers from trigger board (penn)
    uint32_t calib_trigger_enable_; // Enable internal triggers (will become calib triggers)
    uint32_t partition_;            // Partition number of this partition [Partition 0 controls 
                                    // the overall functions]

    std::string zmq_conn_;  // String specifying the zmq connection to subscribe for inhibit information
  };
}

#endif /* dune_artdaq_Generators_TimingReceiver_hh */
