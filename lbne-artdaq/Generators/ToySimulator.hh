#ifndef lbne_artdaq_Generators_ToySimulator_hh
#define lbne_artdaq_Generators_ToySimulator_hh

// ToySimulator is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, obtaining data from the ToyHardwareInterface class

// ToySimulator is designed to simulate values coming in from one of
// two types of digitizer boards, one called "TOY1" and the other
// called "TOY2"; the only difference between the two boards is the #
// of bits in the ADC values they send. These values are declared as
// FragmentType enum's in lbne-artdaq's
// lbne-raw-data/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "lbne-raw-data/Overlays/ToyFragment.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"

#include "ToyHardwareInterface/ToyHardwareInterface.hh"

#include <random>
#include <vector>
#include <atomic>

namespace lbne {    

  class ToySimulator : public artdaq::CommandableFragmentGenerator {
  public:
    explicit ToySimulator(fhicl::ParameterSet const & ps);
    ~ToySimulator();

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    // The start, stop and stopNoMutex methods are declared pure
    // virtual in CommandableFragmentGenerator and therefore MUST be
    // overridden; note that stopNoMutex() doesn't do anything here

    void start() override;
    void stop() override;
    void stopNoMutex() override {}

    std::unique_ptr<ToyHardwareInterface> hardware_interface_; 

    ToyFragment::Metadata metadata_;

    // buffer_ points to the buffer which the hardware interface will
    // fill. Notice that it's a raw pointer rather than a smart
    // pointer as the API to ToyHardwareInterface was chosen to be a
    // C++03-style API for greater realism

    char* readout_buffer_;

    FragmentType fragment_type_;
    bool throw_exception_;
  };
}

#endif /* lbne_artdaq_Generators_ToySimulator_hh */
