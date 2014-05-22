#ifndef lbne_artdaq_Generators_TpcRceReceiver_hh
#define lbne_artdaq_Generators_TpcRceReceiver_hh

// TpcRceReceiver is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, generating all ADC counts with equal probability via
// the std::uniform_int_distribution class

// TpcRceReceiver is designed to simulate values coming in from one of
// two types of digitizer boards, one called "TOY1" and the other
// called "TOY2"; the only difference between the two boards is the #
// of bits in the ADC values they send. These values are declared as
// FragmentType enum's in lbne-artdaq's
// lbne-artdaq/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq/DAQdata/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "lbne-artdaq/Overlays/MilliSliceFragment.hh"
#include "lbne-artdaq/Overlays/FragmentType.hh"
#include "lbne-artdaq/Generators/RceSupportLib/RceDataReceiver.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>

namespace lbne {    

  class TpcRceReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TpcRceReceiver(fhicl::ParameterSet const & ps);

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;


    // State transition methods, for future use, if/when needed
    void start() override; // {}
    void stop() override; // {}
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

    uint16_t udp_receive_port_;
    size_t raw_buffer_size_;
    uint32_t raw_buffer_precommit_;

    bool run_receiver_;
    std::unique_ptr<lbne::RceDataReceiver> data_receiver_;

    uint32_t format_millislice_from_raw_buffer(uint16_t* src_addr, size_t src_size,
    		                                   uint8_t* dest_addr, size_t dest_size);


  };
}

#endif /* lbne_artdaq_Generators_TpcRceReceiver_hh */
