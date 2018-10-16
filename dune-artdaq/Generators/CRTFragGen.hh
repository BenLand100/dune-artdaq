/* Author: Matthew Strait <mstrait@fnal.gov> */

#ifndef artdaq_Generators_CRTFragGen_hh
#define artdaq_Generators_CRTFragGen_hh

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "CRTInterface/CRTInterface.hh"

#include <random>
#include <vector>
#include <atomic>
#include <memory> //For std::unique_ptr

#include "uhal/uhal.hpp"

namespace CRT
{
  class FragGen : public artdaq::CommandableFragmentGenerator
  {
    public:

    /**
      \brief Initialize the CRT DAQ.
      In fact, start the whole backend DAQ up, but do not start
      passing data to artdaq yet.
    */
    explicit FragGen(fhicl::ParameterSet const& ps);
    virtual ~FragGen();

    private:

    /**
     * \brief The "getNext_" function is used to implement user-specific
     * functionality; it's a mandatory override of the pure virtual
     * getNext_ function declared in CommandableFragmentGenerator
     * \param output New FragmentPtrs will be added to this container
     * \return True if data-taking should continue
     */
    bool getNext_(std::list< std::unique_ptr<artdaq::Fragment> > & output) override;

    /**
     * \brief Perform start actions
     */
    void start() override;

    /** \brief Perform stop actions */
    void stop() override;

    void stopNoMutex() override {}

    // Name of CRT database table that gives the information about the set of modules
    // we will read from.
    std::string sqltable;

    // Written to by the hardware interface
    char* readout_buffer_;

    std::unique_ptr<CRTInterface> hardware_interface_;

    /*********************************************************************
       And then all the rest of the members are here to deal with fixing
       the CRT hardware's 32-bit time stamp.
    *********************************************************************/

    // Gets the full 64-bit run start time, measured in 50MHz clock ticks,
    // from a timing board and puts it in runstarttime.
    void getRunStartTime();

    // uint64_t (after unwinding a few layers of typedefs) for the
    // global clock.  For the CRT, we assemble this out of the 32-bit
    // timestamp directly kept by the CRT hardware and the timestamp of
    // the beginning of the run, keeping track of rollovers of the lower
    // 32 bits.
    artdaq::Fragment::timestamp_t timestamp_;

    // The upper 32 bits of the timestamp. We keep track of this in the
    // fragment generator because the CRT hardware only keeps the lower
    // 32 bits.
    uint32_t uppertime;

    // The previous 32-bit timestamp received from the CRT hardware (or
    // the run start time if no events yet), so we can determine if we
    // rolled over and need to increment uppertime.
    uint32_t oldlowertime = 0;

    // The 64-bit global timestamp of the start of the run. We need to
    // retrieve and store this to repair the CRT's internal 32-bit time.
    uint64_t runstarttime;

    // The partition number from the FCL.  We need to write this into the
    // timing board to be able to retrieve the run start time.
    int partition_number;

    // True if this process is the one designated to start the backend DAQ.
    bool startbackend;

    //Keep track of which USB board this board reader is reading from for debugging
    const std::string fUSBString;

    std::string timingXMLfilename;
    std::string timinghardwarename;
  };
}

#endif /* artdaq_Generators_CRTFragGen_hh */
