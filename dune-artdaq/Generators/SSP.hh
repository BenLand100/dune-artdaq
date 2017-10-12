#ifndef artdaq_dune_Generators_SSP_hh
#define artdaq_dune_Generators_SSP_hh

// SSP is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, generating all ADC counts with equal probability via
// the std::uniform_int_distribution class

// SSP is designed to simulate values coming in from one of
// two types of digitizer boards, one called "TOY1" and the other
// called "TOY2"; the only difference between the two boards is the #
// of bits in the ADC values they send. These values are declared as
// FragmentType enum's in artdaq-dune's
// artdaq-dune/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "dune-artdaq/Generators/anlBoard/DeviceInterface.h"

#include <random>
#include <vector>
#include <atomic>

namespace dune {    

  class SSP : public artdaq::CommandableFragmentGenerator {
  public:
    explicit SSP(fhicl::ParameterSet const & ps);

  protected:
    std::string metricsReportingInstanceName() const {
      return instance_name_for_metrics_;
    }

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    virtual void start() override;
    virtual void stop() override;
    virtual void stopNoMutex() override {};

    void ConfigureDevice(fhicl::ParameterSet const & ps);

    void ConfigureDAQ(fhicl::ParameterSet const & ps);

    void BuildChannelControlRegisters(fhicl::ParameterSet const& ps,std::vector<unsigned int>& reg);

    //    virtual void pause();

    //    virtual void resume();

    // FHiCL-configurable variables. Note that the C++ variable names
    // are the FHiCL variable names with a "_" appended

    //FHiCL parameters
    dune::detail::FragmentType const fragment_type_; // Type of fragment (see FragmentType.hh)
    unsigned int board_id_;
    SSPDAQ::Comm_t  interface_type_;

    //Pointer to device interface defined in anlBoard library
    SSPDAQ::DeviceInterface* device_interface_;

    unsigned long fNNoFragments;
    unsigned long fNFragmentsSent;
    unsigned long fNGetNextCalls;

    std::string instance_name_for_metrics_;
  };
}

#endif /* artdaq_dune_Generators_SSP_hh */
