#ifndef dune_artdaq_Overlays_TpcNanoSlice_hh
#define dune_artdaq_Overlays_TpcNanoSlice_hh

#include <stdint.h>
#include <stddef.h>

namespace dune {
  class TpcNanoSlice;
}

class dune::TpcNanoSlice {

public:

  struct Header {

	  typedef uint64_t data_t;

	  typedef uint8_t format_version_t;
	  typedef uint64_t nova_timestamp_t;
	  typedef uint32_t rce_counter_t;
	  typedef uint32_t frontend_counter_t;

	  typedef uint8_t data_packet_type_t;
	  typedef bool misalign_detected_t;
	  typedef bool pgp_lane_tag_id_error_t;
	  typedef bool pgp_lane_sync_error_t;
	  typedef bool data_compressed_t;
	  typedef bool pgp_lane_masked_t;
	  typedef uint32_t rtt_sync_delay_t;
	  typedef uint64_t hit_bit_map_t;

	  static constexpr size_t raw_header_words = 1;
	  data_t raw_header_data[raw_header_words];
  };

  typedef uint64_t raw_data_word_t;
  typedef uint16_t nanoslice_size_t;
  typedef uint16_t sample_count_t;

  enum pgp_lane_id {
	  pgp_lane_0 = 0,
	  pgp_lane_1,
	  pgp_lane_2,
	  pgp_lane_3,
	  pgp_lane_max
  };

  enum hit_bit_map_range {
	  hit_bit_map_low = 0,
	  hit_bit_map_high,
	  hit_bit_map_max,
  };

  // This constructor accepts a memory buffer that contains an existing
  // nanoSlice and allows the the data inside it to be accessed
  TpcNanoSlice(uint8_t* address);
  //this constructor sets the run mode
  // 
  TpcNanoSlice(uint8_t* address, uint8_t mode);

  // Returns the format version field from the header
  Header::format_version_t format_version() const;

  // Returns the NOVA timestamp field from the header
  Header::nova_timestamp_t nova_timestamp() const;

  // Returns the RCE counter field from the header
  Header::rce_counter_t rce_counter() const;

  // Returns the frontend counter field from the header
  Header::frontend_counter_t frontend_counter() const;

  // Returns the data packet type from the header
  Header::data_packet_type_t data_packet_type() const;

  // Returns the misalignment detected flag from the header
  Header::misalign_detected_t misalign_detected() const;

  // Returns to PGP lane Tag ID error flag from the header
  Header::pgp_lane_tag_id_error_t pgp_lane_tag_id_error() const;

  // Returns the PGP lane sync error from the header
  Header::pgp_lane_sync_error_t pgp_lane_sync_error(pgp_lane_id const lane) const;

  // Returns the data compression flag from the header
  Header::data_compressed_t data_compressed() const;

  // Returns the PGP lane masked bits from the header
  Header::pgp_lane_masked_t pgp_lane_masked(pgp_lane_id const lane) const;

  // Returns the round_trip sync delay over PGP from the header
  Header::rtt_sync_delay_t rtt_sync_delay() const;

  // Returns the hit bit maps from the header
  Header::hit_bit_map_t hit_bit_map(hit_bit_map_range const) const;

  // Returns the size of the TpcNanoSlice
  dune::TpcNanoSlice::nanoslice_size_t size() const;

  // Returns the number of samples in the nanoslice
  sample_count_t sampleCount() const;

  // Fetches the value for the requested sample.  Returns true if
  // the requested value was found, false if not.
  bool sampleValue(uint32_t channel, uint16_t& value) const;

  static uint64_t getMask(int param){
	uint64_t mask=0;
	mask = (1 << param) - 1;//sets the mask to 0000...11111...11
	return mask;
  };

  uint64_t* raw() const;

  uint32_t  getNChannels() const {return  num_channels;};
  uint8_t   getRunMode() const {return runMode;};
protected:

  // returns a pointer to the header
  Header const* header_() const;

  // returns a pointer to the first sample word
  uint64_t const* data_() const;

  uint8_t* buffer_;


private:  
  uint8_t runMode;

  nanoslice_size_t raw_payload_words_compressed;
  nanoslice_size_t raw_payload_words_uncompressed;

  uint32_t num_channels;

};

#endif /* dune_artdaq_Overlays_TpcNanoSlice_hh */
