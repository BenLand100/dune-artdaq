/* Author: Matthew Strait <mstrait@fnal.gov> */

#ifndef dune_artdaq_Generators_CRTFragGen_hh
#define dune_artdaq_Generators_CRTFragGen_hh

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"

#include "CRTInterface/CRTInterface.hh"

#include <random>
#include <vector>
#include <atomic>

namespace dune
{
  class CRTFragGen : public artdaq::CommandableFragmentGenerator
  {
    public:

    explicit CRTFragGen(fhicl::ParameterSet const& ps);
    virtual ~CRTFragGen();

    private:

    /**
     * \brief The "getNext_" function is used to implement user-specific
     * functionality; it's a mandatory override of the pure virtual
     * getNext_ function declared in CommandableFragmentGenerator
     * \param output New FragmentPtrs will be added to this container
     * \return True if data-taking should continue
     */
    bool getNext_(std::list< std::unique_ptr<artdaq::Fragment> > & output) override;

    // The start, stop and stopNoMutex methods are declared pure
    // virtual in CommandableFragmentGenerator and therefore MUST be
    // overridden; note that stopNoMutex() doesn't do anything here

    /**
     * \brief Perform start actions
     * Override of pure virtual function in CommandableFragmentGenerator.
     */
    void start() override;

    /** \brief Perform stop actions
    * Override of pure virtual function in CommandableFragmentGenerator.  */
    void stop() override;

    /** \brief Override of pure virtual function in CommandableFragmentGenerator.
    * stopNoMutex does not do anything in dune::CRTFragGen */
    void stopNoMutex() override {}

    std::unique_ptr<CRTInterface> hardware_interface_;

    // I don't know what kind of a time this is, except that it is a uint64_t
    artdaq::Fragment::timestamp_t timestamp_;

    // Written to by the hardware interface
    char* readout_buffer_;
  };
}

#endif /* dune_artdaq_Generators_CRTFragGen_hh */
