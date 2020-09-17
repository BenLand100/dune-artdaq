#ifndef dune_artdaq_Generators_FelixReceiver_hh
#define dune_artdaq_Generators_FelixReceiver_hh

// FelixReceiver is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, obtaining data from the FelixHardwareInterface class

// FelixReceiver is designed to simulate values coming in from one of
// two types of digitizer boards, one called "TOY1" and the other
// called "TOY2"; the only difference between the two boards is the #
// of bits in the ADC values they send. These values are declared as
// FragmentType enum's in artdaq-demo's
// artdaq-core-demo/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
//#include "artdaq-core/Data/Fragments.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/CPUHitsFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "Felix/FelixHardwareInterface.hh"
#include "Felix/FelixOnHostInterface.hh"

#include <random>
#include <vector>
#include <atomic>

namespace dune {

  class FelixReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit FelixReceiver(fhicl::ParameterSet const & ps);
    ~FelixReceiver();

  protected:
    std::string metricsReportingInstanceName() const {
      return instance_name_for_metrics_;
    }

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

    bool configured;

    // RS -> I know it's not the best, but for now this is how we support 2 modes.
    // Based on configuration on_mode_ we either create flx_hw or netio_hw
    std::unique_ptr<FelixOnHostInterface> flx_hardware_interface_;

    // Configurable HWInterface. Can take care of multiple links.
    std::unique_ptr<FelixHardwareInterface> netio_hardware_interface_;
    // Exposed infos from HWInterace to pre-allocate ArtDAQ fragment with correct size.
    unsigned message_size_;
    unsigned trigger_window_size_;

    artdaq::Fragment::timestamp_t timestamp_;
    int timestampScale_;

    FelixFragmentBase::Metadata metadata_;
    CPUHitsFragment::Metadata metadata_hits_;

    // buffer_ points to the buffer which the hardware interface will
    // fill. Notice that it's a raw pointer rather than a smart
    // pointer as the API to FelixHardwareInterface was chosen to be a
    // C++03-style API for greater realism

    size_t frame_size_;
    std::string op_mode_;
    unsigned num_links_;
    //char* readout_buffer_;
    bool onhost;


    FragmentType fragment_type_;       // The fragment type of the raw data fragment
    FragmentType fragment_type_hits_;  // The fragment type of the hits fragment
    std::vector<uint16_t> flx_frag_ids_;

    // Metrics
    std::string instance_name_for_metrics_;
    unsigned metrics_report_time_;
    size_t num_frags_m_;
    

  };
}

#endif /* artdaq_demo_Generators_FelixReceiver_hh */
