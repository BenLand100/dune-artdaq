#ifndef lbne_artdaq_Generators_PennReceiver_hh
#define lbne_artdaq_Generators_PennReceiver_hh

// PennReceiver is a simple type of fragment generator intended to be
// studied by new users of artdaq as an example of how to create such
// a generator in the "best practices" manner. Derived from artdaq's
// CommandableFragmentGenerator class, it can be used in a full DAQ
// simulation, generating all ADC counts with equal probability via
// the std::uniform_int_distribution class

// PennReceiver is designed to simulate values coming in from one of
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
#include "lbne-raw-data/Overlays/MilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"
#include "lbne-artdaq/Generators/pennBoard/PennClient.hh"
#include "lbne-artdaq/Generators/pennBoard/PennDataReceiver.hh"
#include "lbne-artdaq/Generators/pennBoard/PennCompileOptions.hh"

#include <random>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <map>
#include <chrono>

namespace lbne {    

typedef struct TriggerMaskConfig {
  std::string id;
  std::string id_mask;
  uint8_t     logic;
  uint8_t     prescale;
  uint64_t    g1_mask_tsu;
  uint64_t    g1_mask_bsu;
  uint8_t     g1_logic;
  uint64_t    g2_mask_tsu;
  uint64_t    g2_mask_bsu;
  uint8_t     g2_logic;
} TriggerMaskConfig;

  class PennReceiver : public artdaq::CommandableFragmentGenerator {
  public:
    explicit PennReceiver(fhicl::ParameterSet const & ps);

  protected:
    std::string metricsReportingInstanceName() const {
      return instance_name_for_metrics_;
    }

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

    ////HARDWARE OPTIONS
    // config stream connection
    std::string penn_client_host_addr_;
    std::string penn_client_host_port_;
    uint32_t	penn_client_timeout_usecs_;

    /// Data buffer options
    // data stream connection
    std::string penn_data_dest_host_;
    uint16_t    penn_data_dest_port_;
    // microslice duration
    uint32_t    penn_data_microslice_size_;
    // Number max number of frames in each microslice
    // FIXME: Check if it is necessary.
    // Probably going to be removed
    uint16_t    penn_data_dest_rollover_;

    /// Channel masks
    uint64_t    penn_channel_mask_bsu_;
    uint64_t    penn_channel_mask_tsu_;

    /// External trigger options
    // These are triggers that are received on the trigger-in channels
    uint8_t     penn_ext_triggers_mask_;
    bool        penn_ext_triggers_echo_;
    uint8_t     penn_ext_triggers_echo_width_;

    /// Calibrations
    uint16_t    penn_calib_period_;
    uint8_t     penn_calib_channel_mask_;
    uint8_t     penn_calib_pulse_width_;



    /// Muon trigger configuration
    // number of trigger masks configured
    uint32_t penn_muon_num_triggers_;
    // size (in clock ticks) of the trigger out pulse
    uint8_t penn_trig_out_pulse_width_;
    // Number of clock ticks to keep input signal high after rise.
    // Accounts for timing offsets between the panels.
    uint32_t penn_trig_in_window_;
    // Number of clock ticks that input signal is forced locked low after
    // a successful pulse. Accounts for afterpulses (reflections?) from the panels.
    uint32_t penn_trig_lockdown_window_;

    std::vector<TriggerMaskConfig>  muon_triggers_;


    ////BOARDREADER OPTIONS
    //
    uint32_t receiver_tick_period_usecs_;
    // millislice size
    uint32_t millislice_size_;
    uint16_t millislice_overlap_size_;
    // boardreader printout
    uint32_t reporting_interval_fragments_;
    uint32_t reporting_interval_time_;
    //buffer sizes
    size_t raw_buffer_size_;
    uint32_t raw_buffer_precommit_;
    size_t empty_buffer_low_mark_;
    bool   use_fragments_as_raw_buffer_;

    ////EMULATOR OPTIONS
    // amount of data to generate
    uint32_t    penn_data_num_millislices_;
    uint32_t    penn_data_num_microslices_;
    float       penn_data_frag_rate_;
    // type of data to generate
    uint16_t    penn_data_payload_mode_;
    uint16_t    penn_data_trigger_mode_;
    int32_t     penn_data_fragment_microslice_at_ticks_;
    // special debug options
    bool        penn_data_repeat_microslices_;
    bool        penn_data_debug_partial_recv_;

    std::map<uint8_t*, std::unique_ptr<artdaq::Fragment>> raw_to_frag_map_;

    std::unique_ptr<lbne::PennClient> penn_client_;

    bool run_receiver_;
    std::unique_ptr<lbne::PennDataReceiver> data_receiver_;

    std::size_t millislices_received_;
    std::size_t total_bytes_received_;
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point report_time_;

    PennRawBufferPtr create_new_buffer_from_fragment(void);
    uint32_t format_millislice_from_raw_buffer(uint16_t* src_addr, size_t src_size,
    		                                   uint8_t* dest_addr, size_t dest_size);
    uint32_t validate_millislice_from_fragment_buffer(uint8_t* data_addr, size_t data_size, 
#ifndef REBLOCK_PENN_USLICE
						      uint32_t us_count,
#endif
						      uint16_t millislice_id,
						      uint16_t payload_count, uint16_t payload_count_counter,
						      uint16_t payload_count_trigger, uint16_t payload_count_timestamp,
						      uint64_t end_timestamp, uint32_t width_in_ticks, uint32_t overlap_in_ticks);

    void generate_config_frag(std::ostringstream& config_frag);
    void generate_config_frag_emulator(std::ostringstream& config_frag);

    std::string instance_name_for_metrics_;
  };

}

#endif /* lbne_artdaq_Generators_PennReceiver_hh */
