#ifndef dune_artdaq_Generators_TriggerBoardReader_hh
#define dune_artdaq_Generators_TriggerBoardReader_hh

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "TriggerBoard/CTB_Controller.hh"

#include <random>
#include <vector>
#include <atomic>


namespace dune {    

  class TriggerBoardReader : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TriggerBoardReader(fhicl::ParameterSet const & ps);
    ~TriggerBoardReader();

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

    std::unique_ptr<CTB_Controller> _run_controller ; 

    // artdaq::Fragment::timestamp_t timestamp_;
    // int timestampScale_;

    // ToyFragment::Metadata metadata_;

    // buffer_ points to the buffer which the hardware interface will
    // fill. Notice that it's a raw pointer rather than a smart
    // pointer as the API to ToyHardwareInterface was chosen to be a
    // C++03-style API for greater realism

    char* readout_buffer_;

    FragmentType fragment_type_;
    bool throw_exception_;
  };
}

#endif /* dune_artdaq_Generators_TriggerBoardReader_hh */
