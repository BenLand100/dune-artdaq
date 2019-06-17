#ifndef dune_artdaq_Overlays_TpcMicroSlice_hh
#define dune_artdaq_Overlays_TpcMicroSlice_hh

#include "dune-raw-data/Overlays/TpcNanoSlice.hh"
#include <memory>

namespace dune {
  class TpcMicroSlice;
}

class dune::TpcMicroSlice {

public:

  struct Header {

	// Generic raw header word type
    typedef uint32_t data_t;

    // Raw header word 0 fields
    typedef uint32_t microslice_size_t;

    // Raw header word 1 fields
    typedef uint32_t sequence_id_t;

    // Raw header word 2 fields
    typedef uint32_t type_id_t;

    // Raw header word 3-4 fields
    typedef uint64_t softmsg_t;

    // Raw header word 5-6 fields
    typedef uint64_t firmmsg_t;

    // Parameters derived from header field translation
    typedef uint32_t nanoslice_count_t;

    //    static constexpr size_t raw_header_words = 8;
    //this number should be 7*32bits from what artdaqbuffer says it sends
    static constexpr size_t raw_header_words = 7;
    static constexpr uint8_t groups_per_microslice = 4;

    // Raw header words
    data_t raw_header_data[raw_header_words];
  };

  // This constructor accepts a memory buffer that contains an existing
  // TpcMicroSlice and allows the the data inside it to be accessed
  TpcMicroSlice(uint8_t* address);

  // Returns the size of the TpcMicroSlice
  Header::microslice_size_t size() const;

  // Returns the sequence ID of the TpcMicroSlice
  Header::sequence_id_t sequence_id() const;

  // Returns the type ID of the TpcMicroSlice
  Header::type_id_t type_id() const;
  //some individual pieces in type_id
  bool errorFlag() const;
  bool softTrig() const;
  bool extTrig() const;
  bool droppedFrame() const;
  bool timeGap() const;
  uint8_t runMode() const;
  uint16_t rceSoftwareVersion() const;

  // Returns the software message of the TpcMicroSlice
  Header::softmsg_t software_message() const;

  // Returns the firmware message of the TpcMicroSlice
  Header::firmmsg_t firmware_message() const;

  // Returns the number of nanoslices in the microslice
  Header::nanoslice_count_t nanoSliceCount() const;

  // Finds the ADC value associated with nanoslice[index] and sample[sample] and
  // assigns it to memory location [value]
  bool nanosliceSampleValue(uint32_t index, uint32_t sample, uint16_t& value) const;

  // Finds the corresponding NOvA timestamp
  dune::TpcNanoSlice::Header::nova_timestamp_t nanosliceNova_timestamp(uint32_t index) const;

  // Returns the requested NanoSlice if the requested slice was found,
  // otherwise returns an empty pointer
  std::unique_ptr<TpcNanoSlice> nanoSlice(uint32_t index) const;

protected:

  // returns a pointer to the header
  Header const* header_() const;

  // returns a pointer to the requested NanoSlice
  uint8_t* data_(uint32_t index) const;

  uint8_t* buffer_;
};

#endif /* dune_artdaq_Overlays_TpcMicroSlice_hh */
