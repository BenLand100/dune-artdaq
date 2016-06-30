#ifndef lbne_artdaq_Generators_TPCMilliSliceSimulatorWithCopy_hh
#define lbne_artdaq_Generators_TPCMilliSliceSimulatorWithCopy_hh

// TPCMilliSliceSimulatorWithCopy is a simple fragment generator
// that is designed to demonstrate the creation of a MilliSlice.
// In particular, it models a scenario where the data can *not*
// be read directly into an artdaq::Fragment.  This might be
// the case when we need to use a third-party library that uses
// its own internal buffering, and we need to copy the data out
// of the library buffer into the artdaq::fragment.

#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include <vector>
#include <stdint.h>

namespace lbne {    

  class TPCMilliSliceSimulatorWithCopy : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TPCMilliSliceSimulatorWithCopy(fhicl::ParameterSet const & ps);

  private:

    // The "getNext_" method returns the next fragment (containing
    // one MilliSlice).  At the moment, only one fragment is
    // returned per call to getNext_().
    bool getNext_(artdaq::FragmentPtrs & output) override;

    // State transition methods, for future use, if/when needed
    void start() override {}
    void stopNoMutex() override {}
    void stop() override {}
    void pause() override {}
    void resume() override {}

    // Reporting functionality, for future use, if/when needed
    std::string report() override { return ""; }

    // data members
    std::vector<artdaq::Fragment::fragment_id_t> fragment_ids_;

    // methods and data that emulate calls to a third-party library
    // to fetch the data from the hardware
    uint32_t number_of_microslices_to_generate_;
    uint32_t number_of_nanoslices_to_generate_;
    uint32_t number_of_values_to_generate_;
    uint32_t simulated_readout_time_usec_;
    const uint32_t THIRD_PARTY_BUFFER_SIZE_ = 4096; // larger for real HW?
    std::vector<uint8_t> work_buffer_;
    // the "pretend read" method" returns the size of the data read
    uint32_t pretend_to_read_a_millislice_from_the_hardware();
    uint8_t* get_start_address_of_third_party_buffer();
  };
}

#endif /* lbne_artdaq_Generators_TPCMilliSliceSimulatorWithCopy_hh */
