#ifndef dune_artdaq_Generators_TpcRceReceiver_hh
#define dune_artdaq_Generators_TpcRceReceiver_hh

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
// FragmentType enum's in dune-artdaq's
// dune-raw-data/Overlays/FragmentType.hh header.

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-raw-data/Overlays/MilliSliceFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-artdaq/Generators/RceSupportLib/RceClient.hh"
#include "dune-artdaq/Generators/RceSupportLib/RceDataReceiver.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

namespace dune {

class TpcRceReceiver : public artdaq::CommandableFragmentGenerator {
 public:
  explicit TpcRceReceiver(fhicl::ParameterSet const& ps);

 protected:
  std::string metricsReportingInstanceName() const {
    return instance_name_for_metrics_;
  }

 private:
  // The "getNext_" function is used to implement user-specific
  // functionality; it's a mandatory override of the pure virtual
  // getNext_ function declared in CommandableFragmentGenerator

  bool getNext_(artdaq::FragmentPtrs& output) override;

  // JCF, Dec-11-2015

  // startOfDatataking will determine whether or not we've begun
  // taking data, either because of a new run (from the start
  // transition) or a new subrun (from the resume transition). It's
  // designed to be used to determine whether no fragments are
  // getting sent downstream (likely because of upstream hardware
  // issues)

  bool startOfDatataking();

  // State transition methods, for future use, if/when needed
  void start() override;  // {}
  void stop() override;   // {}
  void stopNoMutex() override;
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

  std::string dpm_client_host_addr_;
  std::string dpm_client_host_port_;
  uint32_t dpm_client_timeout_usecs_;

  bool dtm_client_enable_;
  std::string dtm_client_host_addr_;
  std::string dtm_client_host_port_;
  uint32_t dtm_client_timeout_usecs_;

  std::string rce_xml_config_file_;
  std::string rce_daq_mode_;
  bool rce_feb_emulation_;
  uint32_t rce_trg_accept_cnt_;
  uint32_t rce_trg_frame_cnt_;
  uint32_t rce_hls_blowoff_;

  std::string rce_data_dest_host_;
  uint16_t rce_data_dest_port_;
  uint32_t rce_data_num_millislices_;
  uint32_t rce_data_num_microslices_;
  float rce_data_frag_rate_;
  uint16_t rce_data_adc_mode_;
  float rce_data_adc_mean_;
  float rce_data_adc_sigma_;

  uint16_t receive_port_;
  size_t raw_buffer_size_;
  uint32_t raw_buffer_precommit_;
  uint32_t filled_buffer_release_max_;
  size_t empty_buffer_low_mark_;
  size_t filled_buffer_high_mark_;
  bool use_fragments_as_raw_buffer_;

  uint32_t receiver_tick_period_usecs_;

  std::map<uint8_t*, std::unique_ptr<artdaq::Fragment>> raw_to_frag_map_;
  uint16_t number_of_microslices_per_millislice_;
  uint16_t number_of_microslices_per_trigger_;

  std::unique_ptr<dune::RceClient> dpm_client_;
  std::unique_ptr<dune::RceClient> dtm_client_;

  std::string instance_name_;

  bool run_receiver_;
  std::unique_ptr<dune::RceDataReceiver> data_receiver_;

  std::size_t millislices_received_;
  std::size_t total_bytes_received_;
  std::chrono::high_resolution_clock::time_point start_time_;
  std::chrono::high_resolution_clock::time_point report_time_;
  uint32_t reporting_interval_fragments_;
  uint32_t reporting_interval_time_;

  RceRawBufferPtr create_new_buffer_from_fragment(void);
  uint32_t format_millislice_from_raw_buffer(uint16_t* src_addr,
                                             size_t src_size,
                                             uint8_t* dest_addr,
                                             size_t dest_size);
  uint32_t validate_millislice_from_fragment_buffer(uint8_t* data_addr,
                                                    size_t data_size,
                                                    uint32_t count);

  std::string instance_name_for_metrics_;
  std::string empty_buffer_low_water_metric_name_;
  std::string empty_buffer_available_metric_name_;
  std::string filled_buffer_high_water_metric_name_;
  std::string filled_buffer_available_metric_name_;

  std::chrono::high_resolution_clock::time_point last_buffer_received_time_;
  uint32_t data_timeout_usecs_;

  std::size_t max_rce_events_;
  std::size_t max_buffer_attempts_;
};
}

#endif /* dune_artdaq_Generators_TpcRceReceiver_hh */
