// FelixFragment.hh edited from ToyFragment.hh by Milo Vermeulen and Roland
// Sipos (2017) to accept WIB frames.

#ifndef artdaq_dune_Overlays_FelixFragment_hh
#define artdaq_dune_Overlays_FelixFragment_hh

#include "FragmentType.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "dune-raw-data/Overlays/FelixHitFormat.hh"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <map>
#include <vector>
#include <zlib.h>

// Implementation of "FelixFragment", an artdaq::FelixFragment overlay
// class used for WIB->FELIX frames.
//
// The intention of this class is to provide an Overlay for a 12-bit ADC
// module.

namespace dune {
// fwd declare FelixFragment
class FelixFragmentBase;
class FelixFragmentUnordered;
class FelixFragmentReordered;
class FelixFragmentCompressed;
class FelixFragmentHits;
class FelixFragment;
}  // namespace dune

//============================
// FELIX fragment base class
//============================
class dune::FelixFragmentBase {
 public:
  /* Struct to hold FelixFragment Metadata. */
  struct Metadata {
    typedef uint32_t data_t;

    // Old metadata.
    // data_t num_frames : 16, reordered : 8, compressed : 8;
    // data_t offset_frames : 16, window_frames : 16;

    data_t control_word : 12, version : 4, reordered : 8, compressed : 8;
    data_t num_frames;
    data_t offset_frames;
    data_t window_frames;

    static size_t const size_words = 4ul; /* Units of Metadata::data_t */

    bool operator==(const Metadata& rhs) {
      return (control_word == rhs.control_word) &&
             (version == rhs.version) &&
             (reordered == rhs.reordered) &&
             (compressed == rhs.compressed) &&
             (num_frames == rhs.num_frames) &&
             (offset_frames == rhs.offset_frames) &&
             (window_frames == rhs.window_frames);
    }
  };

  struct Old_Metadata {
    typedef uint32_t data_t;
    data_t num_frames : 16, reordered : 8, compressed : 8;
    data_t offset_frames : 16, window_frames : 16;

    bool operator==(const Metadata& rhs) {
      return (num_frames == rhs.num_frames) &&
             (reordered == rhs.reordered) &&
             (compressed == rhs.compressed) &&
             (offset_frames == rhs.offset_frames) &&
             (window_frames == rhs.window_frames);
    }
  };

  static_assert(sizeof(Metadata) ==
                    Metadata::size_words * sizeof(Metadata::data_t),
                "FelixFragment::Metadata size changed");

  /* Frame field and accessors. */
  virtual uint8_t sof(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t version(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t fiber_no(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t slot_no(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t crate_no(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t mm(const unsigned& frame_ID = 0) const = 0;
  virtual uint8_t oos(const unsigned& frame_ID = 0) const = 0;
  virtual uint16_t wib_errors(const unsigned& frame_ID = 0) const = 0;
  virtual uint64_t timestamp(const unsigned& frame_ID = 0) const = 0;
  virtual uint16_t wib_counter(const unsigned& frame_ID = 0) const = 0;

  /* Coldata block accessors. */
  virtual uint8_t s1_error(const unsigned& frame_ID,
                           const uint8_t& block_num) const = 0;
  virtual uint8_t s2_error(const unsigned& frame_ID,
                           const uint8_t& block_num) const = 0;
  virtual uint16_t checksum_a(const unsigned& frame_ID,
                              const uint8_t& block_num) const = 0;
  virtual uint16_t checksum_b(const unsigned& frame_ID,
                              const uint8_t& block_num) const = 0;
  virtual uint16_t coldata_convert_count(const unsigned& frame_ID,
                                         const uint8_t& block_num) const = 0;
  virtual uint16_t error_register(const unsigned& frame_ID,
                                  const uint8_t& block_num) const = 0;
  virtual uint8_t hdr(const unsigned& frame_ID, const uint8_t& block_num,
                      const uint8_t& hdr_num) const = 0;

  // Functions to return a certain ADC value.
  virtual adc_t get_ADC(const unsigned& frame_ID, const uint8_t block_ID,
                        const uint8_t channel_ID) const = 0;
  virtual adc_t get_ADC(const unsigned& frame_ID,
                        const uint8_t channel_ID) const = 0;

  // Function to return all ADC values for a single channel.
  virtual adc_v get_ADCs_by_channel(const uint8_t block_ID,
                                    const uint8_t channel_ID) const = 0;
  virtual adc_v get_ADCs_by_channel(const uint8_t channel_ID) const = 0;
  // Function to return all ADC values for all channels in a map.
  virtual std::map<uint8_t, adc_v> get_all_ADCs() const = 0;

  // Function to print all timestamps.
  virtual void print_timestamps() const = 0;

  virtual void print(const unsigned i) const = 0;

  virtual void print_frames() const = 0;

  FelixFragmentBase(const artdaq::Fragment& fragment)
      : meta_(*(fragment.metadata<Metadata>())),
        artdaq_Fragment_(fragment.dataBeginBytes()),
        sizeBytes_(fragment.dataSizeBytes()) {

    // Check whether the metadata is of the old format.
    if(meta_.control_word != 0xabc && meta_.control_word != 0xcba) {
      // mf::LogInfo("dune::FelixFragment") << "Fragment has old metadata format.";
      const Old_Metadata* old_meta = fragment.metadata<Old_Metadata>();
      meta_ = {0,
               0,
               old_meta->reordered,
               old_meta->compressed,
               old_meta->num_frames,
               old_meta->offset_frames,
               old_meta->window_frames};
    }

    // Check whether current metadata makes sense and guess the format otherwise.
    if(meta_.reordered > 1 || meta_.compressed > 1 || meta_.num_frames < meta_.window_frames) {
      // Assume 6024 frames in a fragment if there is no meaningful metadata.
      meta_.num_frames = 6024;
      meta_.offset_frames = 500;
      meta_.window_frames = 6000;
      // Try to predict reordered/compressed from fragment size.
      unsigned sizeBytes = fragment.dataSizeBytes();
      if(sizeBytes == meta_.num_frames * 464) {
        meta_.reordered = 0;
        meta_.compressed = 0;
      } else if (sizeBytes > meta_.num_frames * 592) {
        // Assume compression factor > 1.
        meta_.reordered = 1;
        meta_.compressed = 0;
      } else {
        // Assume reordered if compressed.
        meta_.reordered = 1;
        meta_.compressed = 1;
      }
    }

    // Try to set the offset to the metadata offset.
    const FelixFrame* first_frame = reinterpret_cast<FelixFrame const*>(artdaq_Fragment_);
    bool shift_good = true;
    // Is the first requested frame in the data?
    shift_good &= fragment.timestamp() - meta_.offset_frames*25 >= first_frame->timestamp();
    // Is the data big enough to accommodate the window size?
    shift_good &= fragment.timestamp() - (meta_.offset_frames + meta_.window_frames)*25 <= first_frame->timestamp() + meta_.num_frames*25;

    if(shift_good) {
      uint64_t remaining_frame_diff = (fragment.timestamp() - first_frame->timestamp())/25 - meta_.offset_frames;
      artdaq_Fragment_ = reinterpret_cast<FelixFrame const*>(artdaq_Fragment_) + remaining_frame_diff;
    } else {
      mf::LogWarning("dune::FelixFragment") << "Can't find the trigger window in FELIX fragment " << fragment.fragmentID() << ".\nFragment TS: " << fragment.timestamp()
                << " first frame TS: " << first_frame->timestamp() << '\n';
    }
  }
  virtual ~FelixFragmentBase() {}

  // The number of words in the current event minus the header.
  virtual size_t total_words() const = 0;
  // The number of frames in the current event.
  virtual size_t total_frames() const = 0;
  // The number of ADC values describing data beyond the header
  virtual size_t total_adc_values() const = 0;
  // Largest ADC value possible
  virtual size_t adc_range(int daq_adc_bits = 12) {
    return (1ul << daq_adc_bits);
  }

  // Raw data access (const only).
  const uint8_t* dataBeginBytes() const {
    return reinterpret_cast<uint8_t const*>(artdaq_Fragment_);
  }
  size_t dataSizeBytes() const { return sizeBytes_; }

 protected:
  Metadata meta_;
  const void* artdaq_Fragment_;
  size_t sizeBytes_;
}; // class dune::FelixFragmentBase


//==================================================
// FELIX fragment for an array of FELIX hits
//==================================================
class dune::FelixFragmentHits : public dune::FelixFragmentBase {
 public:

  // The following structure is overlayed onto the data in the
  // fragment, starting at the beginning.
  struct Body {
    uint64_t timestamp;
    uint32_t window_offset;
    uint8_t fiber_no;
    uint8_t slot_no;
    uint8_t crate_no;
    uint32_t nhits;
      // We have an array of TriggerPrimitive of unknown size
      // beginning in memory at this point, so use a "flexible array
      // member". Not sure how to do this more nicely, when we have to
      // have contiguous memory for the fragment
      dune::TriggerPrimitive primitives[];
  };
    
  // The constructor simply sets its const private member "artdaq_Fragment_"
  // to refer to the artdaq::Fragment object
  FelixFragmentHits(artdaq::Fragment const& fragment)
      : FelixFragmentBase(fragment) {}

  // Here are the getters
  uint64_t get_timestamp() const  { return body_()->timestamp;  }
  uint32_t get_window_offset() const  { return body_()->window_offset;  }
  uint8_t get_fiber_no() const { return body_()->fiber_no; }
  uint8_t get_slot_no() const { return body_()->slot_no; }
  uint8_t get_crate_no() const { return body_()->crate_no; }

  uint32_t get_nhits() const    { return body_()->nhits;    }
  const dune::TriggerPrimitive& get_primitive(int i) const { return body_()->primitives[i]; }
  dune::TriggerPrimitive& get_primitive(int i) { return body_()->primitives[i]; }

  void set_timestamp(uint64_t timestamp) { body_()->timestamp=timestamp;  }
  void set_window_offset(uint32_t offset) { body_()->window_offset=offset;  }
  void set_fiber_no(uint8_t fiber_no) { body_()->fiber_no=fiber_no; }
  void set_slot_no(uint8_t slot_no) { body_()->slot_no=slot_no; }
  void set_crate_no(uint8_t crate_no) { body_()->crate_no=crate_no; }

  void set_nhits(uint32_t nhits) {  body_()->nhits=nhits;    }

  /* Frame field and accessors. */
  virtual uint8_t sof(const unsigned& ) const { return 0; }
  virtual uint8_t version(const unsigned& ) const { return 0; }
  virtual uint8_t fiber_no(const unsigned& ) const { return 0; }
  virtual uint8_t slot_no(const unsigned& ) const { return 0; }
  virtual uint8_t crate_no(const unsigned& ) const { return 0; }
  virtual uint8_t mm(const unsigned& ) const { return 0; }
  virtual uint8_t oos(const unsigned& ) const { return 0; }
  virtual uint16_t wib_errors(const unsigned& ) const { return 0; }
  virtual uint64_t timestamp(const unsigned& ) const { return 0; }
  virtual uint16_t wib_counter(const unsigned& ) const { return 0; }

  /* Coldata block accessors. */
  virtual uint8_t s1_error(const unsigned&,
                           const uint8_t&) const { return 0; }
  virtual uint8_t s2_error(const unsigned&,
                           const uint8_t&) const { return 0; }
  virtual uint16_t checksum_a(const unsigned&,
                              const uint8_t&) const { return 0; }
  virtual uint16_t checksum_b(const unsigned&,
                              const uint8_t&) const { return 0; }
  virtual uint16_t coldata_convert_count(const unsigned&,
                                         const uint8_t&) const { return 0; }
  virtual uint16_t error_register(const unsigned&,
                                  const uint8_t&) const { return 0; }
  virtual uint8_t hdr(const unsigned&, const uint8_t&,
                      const uint8_t&) const { return 0; }

  // Functions to return a certain ADC value.
  virtual adc_t get_ADC(const unsigned&, const uint8_t,
                        const uint8_t) const { return 0; }
  virtual adc_t get_ADC(const unsigned&,
                        const uint8_t) const { return 0; }

  // Function to return all ADC values for a single channel.
  virtual adc_v get_ADCs_by_channel(const uint8_t,
                                    const uint8_t) const { return {}; }
    virtual adc_v get_ADCs_by_channel(const uint8_t) const { return {}; }
  // Function to return all ADC values for all channels in a map.
    virtual std::map<uint8_t, adc_v> get_all_ADCs() const { return {}; }

  // Function to print all timestamps.
  virtual void print_timestamps() const {};

  virtual void print(const unsigned) const {};

  virtual void print_frames() const {};

  virtual ~FelixFragmentHits() {}

  // The number of words in the current event minus the header.
  virtual size_t total_words() const { return 0; }
  // The number of frames in the current event.
  virtual size_t total_frames() const { return 0; }
  // The number of ADC values describing data beyond the header
  virtual size_t total_adc_values() const { return 0; }

 protected:
    // artdaq_Fragment_ already points to dataBeginBytes(), thanks to the FelixFragmentBase constructor
    Body const * body_() const {
        return reinterpret_cast<FelixFragmentHits::Body const *>( artdaq_Fragment_);
    }
    Body * body_() {
        return reinterpret_cast<FelixFragmentHits::Body*>(const_cast<void*>(artdaq_Fragment_));
    }

}; // class dune::FelixFragmentHits

//==================================================
// FELIX fragment for an array of bare FELIX frames
//==================================================
class dune::FelixFragmentUnordered : public dune::FelixFragmentBase {
 public:
  /* Frame field and accessors. */
  uint8_t sof(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->sof();
  }
  uint8_t version(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->version();
  }
  uint8_t fiber_no(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->fiber_no();
  }
  uint8_t slot_no(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->slot_no();
  }
  uint8_t crate_no(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->crate_no();
  }
  uint8_t mm(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->mm();
  }
  uint8_t oos(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->oos();
  }
  uint16_t wib_errors(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->wib_errors();
  }
  uint64_t timestamp(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->timestamp();
  }
  uint16_t wib_counter(const unsigned& frame_ID = 0) const {
    return frame_(frame_ID)->wib_counter();
  }

  /* Coldata block accessors. */
  uint8_t s1_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return frame_(frame_ID)->s1_error(block_num);
  }
  uint8_t s2_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return frame_(frame_ID)->s2_error(block_num);
  }
  uint16_t checksum_a(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return frame_(frame_ID)->checksum_a(block_num);
  }
  uint16_t checksum_b(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return frame_(frame_ID)->checksum_b(block_num);
  }
  uint16_t coldata_convert_count(const unsigned& frame_ID,
                                 const uint8_t& block_num) const {
    return frame_(frame_ID)->coldata_convert_count(block_num);
  }
  uint16_t error_register(const unsigned& frame_ID,
                          const uint8_t& block_num) const {
    return frame_(frame_ID)->error_register(block_num);
  }
  uint8_t hdr(const unsigned& frame_ID, const uint8_t& block_num,
              const uint8_t& hdr_num) const {
    return frame_(frame_ID)->hdr(block_num, hdr_num);
  }

  // Functions to return a certain ADC value.
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t block_ID,
                const uint8_t channel_ID) const {
    return frame_(frame_ID)->channel(block_ID, channel_ID);
  }
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t channel_ID) const {
    return frame_(frame_ID)->channel(channel_ID);
  }

  // Function to return all ADC values for a single channel.
  adc_v get_ADCs_by_channel(const uint8_t block_ID,
                            const uint8_t channel_ID) const {
    adc_v output(total_frames());
    for (size_t i = 0; i < total_frames(); i++) {
      output[i] = get_ADC(i, block_ID, channel_ID);
    }
    return output;
  }
  adc_v get_ADCs_by_channel(const uint8_t channel_ID) const {
    adc_v output(total_frames());
    for (size_t i = 0; i < total_frames(); i++) {
      output[i] = get_ADC(i, channel_ID);
    }
    return output;
  }
  // Function to return all ADC values for all channels in a map.
  std::map<uint8_t, adc_v> get_all_ADCs() const {
    std::map<uint8_t, adc_v> output;
    for (int i = 0; i < 256; i++)
      output.insert(std::pair<uint8_t, adc_v>(i, get_ADCs_by_channel(i)));
    return output;
  }

  // Function to print all timestamps.
  void print_timestamps() const {
    for (unsigned int i = 0; i < total_frames(); i++) {
      std::cout << std::hex << frame_(i)->timestamp() << '\t' << std::dec << i
                << std::endl;
    }
  }

  void print(const unsigned i) const { frame_(i)->print(); }

  void print_frames() const {
    for (unsigned i = 0; i < total_frames(); i++) {
      frame_(i)->print();
    }
  }

  // The constructor simply sets its const private member "artdaq_Fragment_"
  // to refer to the artdaq::Fragment object
  FelixFragmentUnordered(artdaq::Fragment const& fragment)
      : FelixFragmentBase(fragment) {}

  // The number of words in the current event minus the header.
  size_t total_words() const { return sizeBytes_ / sizeof(word_t); }
  // The number of frames in the current event.
  size_t total_frames() const {
    return meta_.window_frames;
  }
  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const {
    return total_frames() * FelixFrame::num_ch_per_frame;
  }

 protected:
  // Allow access to individual frames according to the FelixFrame structure.
  FelixFrame const* frame_(const unsigned& frame_num = 0) const {
    // WARNING: CUSTOM OFFSET OF EIGHT WORDS FOR DEBUGGING -- backed out as it was not needed
    //return reinterpret_cast<dune::FelixFrame const*>(static_cast<uint32_t const*>(artdaq_Fragment_)+8) + frame_num;
    return reinterpret_cast<dune::FelixFrame const*>(static_cast<uint32_t const*>(artdaq_Fragment_)) + frame_num;
  }
}; // class dune::FelixFragmentUnordered

//=======================================================
// FELIX fragment for an array of reordered FELIX frames
//=======================================================
class dune::FelixFragmentReordered : public dune::FelixFragmentBase {
 public:
  /* Frame field and accessors. */
  uint8_t sof(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->sof;
  }
  uint8_t version(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->version;
  }
  uint8_t fiber_no(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->fiber_no;
  }
  uint8_t slot_no(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->slot_no;
  }
  uint8_t crate_no(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->crate_no;
  }
  uint8_t mm(const unsigned& frame_ID = 0) const { return head_(frame_ID)->mm; }
  uint8_t oos(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->oos;
  }
  uint16_t wib_errors(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->wib_errors;
  }
  uint64_t timestamp(const unsigned& frame_ID = 0) const {
    uint64_t result = head_(frame_ID)->timestamp();
    if(header_is_faulty(frame_ID) == false) {
      // Deduce timestamp from first header,
      result += frame_ID * 25;
    }
    return result;
  }
  uint16_t wib_counter(const unsigned& frame_ID = 0) const {
    return head_(frame_ID)->wib_counter();
  }

  /* Coldata block accessors. */
  uint8_t s1_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return blockhead_(frame_ID, block_num)->s1_error;
  }
  uint8_t s2_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return blockhead_(frame_ID, block_num)->s2_error;
  }
  uint16_t checksum_a(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return blockhead_(frame_ID, block_num)->checksum_a();
  }
  uint16_t checksum_b(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return blockhead_(frame_ID, block_num)->checksum_b();
  }
  uint16_t coldata_convert_count(const unsigned& frame_ID,
                                 const uint8_t& block_num) const {
    uint16_t result = blockhead_(frame_ID, block_num)->coldata_convert_count;
    if(header_is_faulty(frame_ID) == false) {
      // Deduce count from first header.
      result += frame_ID;
    }
    return result;
  }
  uint16_t error_register(const unsigned& frame_ID,
                          const uint8_t& block_num) const {
    return blockhead_(frame_ID, block_num)->error_register;
  }
  uint8_t hdr(const unsigned& frame_ID, const uint8_t& block_num,
              const uint8_t& hdr_num) const {
    return blockhead_(frame_ID, block_num)->hdr(hdr_num);
  }

  // Functions to return a certain ADC value.
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t block_ID,
                const uint8_t channel_ID) const {
    return channel_(frame_ID, block_ID * 64 + channel_ID);
  }
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t channel_ID) const {
    return channel_(frame_ID, channel_ID);
  }

  // Function to return all ADC values for a single channel.
  adc_v get_ADCs_by_channel(const uint8_t block_ID,
                            const uint8_t channel_ID) const {
    adc_v output(total_frames());
    for (size_t i = 0; i < total_frames(); i++) {
      output[i] = get_ADC(i, block_ID, channel_ID);
    }
    return output;
  }
  adc_v get_ADCs_by_channel(const uint8_t channel_ID) const {
    return get_ADCs_by_channel(channel_ID/64, channel_ID%64);
  }
  // Function to return all ADC values for all channels in a map.
  std::map<uint8_t, adc_v> get_all_ADCs() const {
    std::map<uint8_t, adc_v> output;
    for (int i = 0; i < 256; i++)
      output.insert(std::pair<uint8_t, adc_v>(i, get_ADCs_by_channel(i)));
    return output;
  }

  // Function to print all timestamps.
  void print_timestamps() const {
    // for (unsigned int i = 0; i < total_frames(); i++) {
    //   std::cout << std::hex << frames_()->timestamp(i) << '\t' << std::dec <<
    //   i
    //             << std::endl;
    // }
  }

  void print(const unsigned i) const {
    std::cout << "Frame " << i
              << " should be printed here.\n"; /* frames_()->print(i); */
  }

  void print_frames() const {
    // for (unsigned i = 0; i < total_frames(); i++) {
    //   frames_()->print(i);
    // }
  }

  // The constructor simply sets its const private member "artdaq_Fragment_"
  // to refer to the artdaq::Fragment object
  FelixFragmentReordered(artdaq::Fragment const& fragment)
      : FelixFragmentBase(fragment), bad_header_num(meta_.num_frames, 0) {
    // Go through the bad headers and assign each a number.
    unsigned int bad_header_count = 1;
    for(unsigned int i = 0; i < total_frames(); ++i) {
      if(header_is_faulty(i)) {
        bad_header_num[i] = bad_header_count++;
      }
      // std::cout << bad_header_num[i] << '\n';
    }
  }

  // The number of words in the current event minus the header.
  size_t total_words() const { return sizeBytes_ / sizeof(word_t); }

  // The number of frames in the current event.
  size_t total_frames() const { return meta_.window_frames; }

  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const {
    return total_frames() * FelixFrame::num_ch_per_frame;
  }

 protected:
  // Important positions within the data buffer.
  const unsigned int adc_start = 0;
  const unsigned int bitlist_start = adc_start + meta_.num_frames * 256 * sizeof(adc_t);
  const unsigned int header_start = bitlist_start + (meta_.num_frames+7)/8;
  // Size of WIB header + four COLDATA headers.
  const unsigned int header_set_size =
      sizeof(dune::WIBHeader) + 4 * sizeof(dune::ColdataHeader);

  // Faulty header information.
  bool header_is_faulty(const unsigned int frame_num) const {
    const uint8_t* curr_byte = static_cast<uint8_t const*>(artdaq_Fragment_) +
                               bitlist_start + frame_num / 8;
    return ((*curr_byte)>>(frame_num%8))&1;
  }
  // Number of the faulty header (0 if header is good).
  std::vector<unsigned int> bad_header_num;

  // Reordered frame format overlaid on the data.
  dune::WIBHeader const* head_(const unsigned int frame_num) const {
    if(header_is_faulty(frame_num)) {
      // Return faulty header.
      return reinterpret_cast<dune::WIBHeader const*>(
                static_cast<uint8_t const*>(artdaq_Fragment_) + header_start +
            bad_header_num[frame_num] * header_set_size);
    } else {
      // Return the first header unchanged if requested header is good.
      return reinterpret_cast<dune::WIBHeader const*>(static_cast<uint8_t const*>(artdaq_Fragment_) + header_start);
    }
  }

  dune::ColdataHeader const* blockhead_(const unsigned int frame_num,
                                        const uint8_t block_num) const {
    if(header_is_faulty(frame_num)) {
      // Return faulty header.
      return reinterpret_cast<dune::ColdataHeader const*>(
                static_cast<uint8_t const*>(artdaq_Fragment_) +
                header_start + sizeof(dune::WIBHeader) + bad_header_num[frame_num] * header_set_size) + block_num;
    } else {
      // Return the first header unchanged if the requested header is good.
      return reinterpret_cast<dune::ColdataHeader const*>(
                static_cast<uint8_t const*>(artdaq_Fragment_) +
                header_start + sizeof(dune::WIBHeader)) + block_num;
    }
  }

  dune::adc_t channel_(const unsigned int frame_num,
                       const uint8_t ch_num) const {
    return *(reinterpret_cast<dune::adc_t const*>(
                 static_cast<uint8_t const*>(artdaq_Fragment_) + adc_start) +
             frame_num + ch_num * meta_.num_frames);
  }
}; // class dune::FelixFragmentReordered

//=======================================
// FELIX fragment for compressed frames.
//=======================================
class dune::FelixFragmentCompressed : public FelixFragmentBase {
 private:
  int decompress_copy(const artdaq::Fragment& src, artdaq::Fragment& dst) {
    // Determine and reserve new fragment size.
    long unsigned int uncompSizeBytes;
    if(meta_.reordered) {
      uncompSizeBytes = meta_.num_frames * (sizeof(dune::WIBHeader) +
                                       4 * sizeof(dune::ColdataHeader) +
                                       256 * sizeof(dune::adc_t));
      // Account for bitfields
      uncompSizeBytes += (meta_.num_frames + 7) / 8;
    } else {
      uncompSizeBytes = meta_.num_frames * sizeof(dune::FelixFrame);
    }
    dst.resizeBytes(uncompSizeBytes);

    int num_bytes = internal_inflate(src.dataBeginBytes(), src.dataSizeBytes(),
                                     dst.dataBeginBytes(), dst.dataSizeBytes());

    return num_bytes;
  }

  int internal_inflate(const void *src, uint srcLen, void *dst, uint dstLen) {
    z_stream strm  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    strm.total_in  = strm.avail_in  = srcLen;
    strm.total_out = strm.avail_out = dstLen;
    strm.next_in   = (Bytef *) src;
    strm.next_out  = (Bytef *) dst;

    strm.zalloc = Z_NULL;
    strm.zfree  = Z_NULL;
    strm.opaque = Z_NULL;

    unsigned read = 0;
    unsigned write = 0;
    int err = 0;

    while(read < srcLen && write < dstLen) {
      strm.next_in = (Bytef*)src + read;
      strm.next_out  = (Bytef *) dst + write;

      unsigned to_read = srcLen - read;
      unsigned to_write = dstLen - write;

      if (to_read < 20) {
        // This cant possibly be a frag
        break;
      }

      strm.total_in  = strm.avail_in  = to_read;
      strm.total_out = strm.avail_out = to_write;

      inflateInit2(&strm, MAX_WBITS | 16);
      err = inflate(&strm, Z_FINISH);
      // Just return the source if data error.
      if(err == Z_DATA_ERROR) {
        std::cout << "Warning: decompression data error. Returning compressed "
                     "fragment.\n";
        memcpy(dst, src, srcLen);
        return srcLen;
      }
      inflateEnd(&strm);

      read += to_read - strm.avail_in;
      write += to_write - strm.avail_out;

      if (err < 0) {
        break;
      }
    }

    return write;
  } 

 public:
  FelixFragmentCompressed(const artdaq::Fragment& fragment) : FelixFragmentBase(fragment) {
    // Decompress.
    decompress_copy(fragment, uncompfrag_);
    // std::cout << decompress_copy(fragment, uncompfrag_) << " returned from decompress_copy\n";
    
    // Update metadata.
    meta_.compressed = 0;
    uncompfrag_.setMetadata(meta_);
    if (meta_.reordered) {
      flxfrag = new FelixFragmentReordered(uncompfrag_);
    } else {
      flxfrag = new FelixFragmentUnordered(uncompfrag_);
    }
  }

  ~FelixFragmentCompressed() { delete flxfrag; }

  /* Frame field and accessors. */
  uint8_t sof(const unsigned& frame_ID = 0) const {
    return flxfrag->sof(frame_ID);
  }
  uint8_t version(const unsigned& frame_ID = 0) const {
    return flxfrag->version(frame_ID);
  }
  uint8_t fiber_no(const unsigned& frame_ID = 0) const {
    return flxfrag->fiber_no(frame_ID);
  }
  uint8_t slot_no(const unsigned& frame_ID = 0) const {
    return flxfrag->slot_no(frame_ID);
  }
  uint8_t crate_no(const unsigned& frame_ID = 0) const {
    return flxfrag->crate_no(frame_ID);
  }
  uint8_t mm(const unsigned& frame_ID = 0) const {
    return flxfrag->mm(frame_ID);
  }
  uint8_t oos(const unsigned& frame_ID = 0) const {
    return flxfrag->oos(frame_ID);
  }
  uint16_t wib_errors(const unsigned& frame_ID = 0) const {
    return flxfrag->wib_errors(frame_ID);
  }
  uint64_t timestamp(const unsigned& frame_ID = 0) const {
    return flxfrag->timestamp(frame_ID);
  }
  uint16_t wib_counter(const unsigned& frame_ID = 0) const {
    return flxfrag->wib_counter(frame_ID);
  }

  /* Coldata block accessors. */
  uint8_t s1_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return flxfrag->s1_error(frame_ID, block_num);
  }
  uint8_t s2_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return flxfrag->s2_error(frame_ID, block_num);
  }
  uint16_t checksum_a(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return flxfrag->checksum_a(frame_ID, block_num);
  }
  uint16_t checksum_b(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return flxfrag->checksum_b(frame_ID, block_num);
  }
  uint16_t coldata_convert_count(const unsigned& frame_ID,
                                 const uint8_t& block_num) const {
    return flxfrag->coldata_convert_count(frame_ID, block_num);
  }
  uint16_t error_register(const unsigned& frame_ID,
                          const uint8_t& block_num) const {
    return flxfrag->error_register(frame_ID, block_num);
  }
  uint8_t hdr(const unsigned& frame_ID, const uint8_t& block_num,
              const uint8_t& hdr_num) const {
    return flxfrag->hdr(frame_ID, block_num, hdr_num);
  }

  // Functions to return a certain ADC value.
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t block_ID,
                const uint8_t channel_ID) const {
    return flxfrag->get_ADC(frame_ID, block_ID, channel_ID);
  }
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t channel_ID) const {
    return flxfrag->get_ADC(frame_ID, channel_ID);
  }

  // Function to return all ADC values for a single channel.
  adc_v get_ADCs_by_channel(const uint8_t block_ID,
                            const uint8_t channel_ID) const {
    return flxfrag->get_ADCs_by_channel(block_ID, channel_ID);
  }
  adc_v get_ADCs_by_channel(const uint8_t channel_ID) const {
    return flxfrag->get_ADCs_by_channel(channel_ID);
  }
  // Function to return all ADC values for all channels in a map.
  std::map<uint8_t, adc_v> get_all_ADCs() const {
    return flxfrag->get_all_ADCs();
  }

  // Function to print all timestamps.
  void print_timestamps() const { return flxfrag->print_timestamps(); }

  void print(const unsigned i) const { return flxfrag->print(i); }

  void print_frames() const { return flxfrag->print_frames(); }

  // The number of words in the current event minus the header.
  size_t total_words() const { return flxfrag->total_words(); }
  // The number of frames in the current event.
  size_t total_frames() const { return flxfrag->total_frames(); }
  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const { return flxfrag->total_adc_values(); }

  // Const function to return the uncompressed fragment.
  const artdaq::Fragment uncompressed_fragment() const { return uncompfrag_; }

 protected:
  // Uncompressed data fragment.
  artdaq::Fragment uncompfrag_;
  // Member pointer to access uncompressed data.
  const FelixFragmentBase* flxfrag;
}; // class dune::FelixFragmentCompressed

//======================
// FELIX fragment class
//======================
class dune::FelixFragment : public FelixFragmentBase {
 public:
  FelixFragment(const artdaq::Fragment& fragment)
      : FelixFragmentBase(fragment) {
    if(meta_.control_word==0xcba){
      flxfrag = new FelixFragmentHits(fragment);
    } else if(meta_.compressed) {
      flxfrag = new FelixFragmentCompressed(fragment);
    } else if (meta_.reordered) {
      flxfrag = new FelixFragmentReordered(fragment);
    } else {
      flxfrag = new FelixFragmentUnordered(fragment);
    }
  }

  ~FelixFragment() { delete flxfrag; }

  /* Frame field and accessors. */
  uint8_t sof(const unsigned& frame_ID = 0) const {
    return flxfrag->sof(frame_ID);
  }
  uint8_t version(const unsigned& frame_ID = 0) const {
    return flxfrag->version(frame_ID);
  }
  uint8_t fiber_no(const unsigned& frame_ID = 0) const {
    return flxfrag->fiber_no(frame_ID);
  }
  uint8_t slot_no(const unsigned& frame_ID = 0) const {
    return flxfrag->slot_no(frame_ID);
  }
  uint8_t crate_no(const unsigned& frame_ID = 0) const {
    return flxfrag->crate_no(frame_ID);
  }
  uint8_t mm(const unsigned& frame_ID = 0) const {
    return flxfrag->mm(frame_ID);
  }
  uint8_t oos(const unsigned& frame_ID = 0) const {
    return flxfrag->oos(frame_ID);
  }
  uint16_t wib_errors(const unsigned& frame_ID = 0) const {
    return flxfrag->wib_errors(frame_ID);
  }
  uint64_t timestamp(const unsigned& frame_ID = 0) const {
    return flxfrag->timestamp(frame_ID);
  }
  uint16_t wib_counter(const unsigned& frame_ID = 0) const {
    return flxfrag->wib_counter(frame_ID);
  }

  /* Coldata block accessors. */
  uint8_t s1_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return flxfrag->s1_error(frame_ID, block_num);
  }
  uint8_t s2_error(const unsigned& frame_ID, const uint8_t& block_num) const {
    return flxfrag->s2_error(frame_ID, block_num);
  }
  uint16_t checksum_a(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return flxfrag->checksum_a(frame_ID, block_num);
  }
  uint16_t checksum_b(const unsigned& frame_ID,
                      const uint8_t& block_num) const {
    return flxfrag->checksum_b(frame_ID, block_num);
  }
  uint16_t coldata_convert_count(const unsigned& frame_ID,
                                 const uint8_t& block_num) const {
    return flxfrag->coldata_convert_count(frame_ID, block_num);
  }
  uint16_t error_register(const unsigned& frame_ID,
                          const uint8_t& block_num) const {
    return flxfrag->error_register(frame_ID, block_num);
  }
  uint8_t hdr(const unsigned& frame_ID, const uint8_t& block_num,
              const uint8_t& hdr_num) const {
    return flxfrag->hdr(frame_ID, block_num, hdr_num);
  }

  // Functions to return a certain ADC value.
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t block_ID,
                const uint8_t channel_ID) const {
    return flxfrag->get_ADC(frame_ID, block_ID, channel_ID);
  }
  adc_t get_ADC(const unsigned& frame_ID, const uint8_t channel_ID) const {
    return flxfrag->get_ADC(frame_ID, channel_ID);
  }

  // Function to return all ADC values for a single channel.
  adc_v get_ADCs_by_channel(const uint8_t block_ID,
                            const uint8_t channel_ID) const {
    return flxfrag->get_ADCs_by_channel(block_ID, channel_ID);
  }
  adc_v get_ADCs_by_channel(const uint8_t channel_ID) const {
    return flxfrag->get_ADCs_by_channel(channel_ID);
  }
  // Function to return all ADC values for all channels in a map.
  std::map<uint8_t, adc_v> get_all_ADCs() const {
    return flxfrag->get_all_ADCs();
  }

  // Function to print all timestamps.
  void print_timestamps() const { return flxfrag->print_timestamps(); }

  void print(const unsigned i) const { return flxfrag->print(i); }

  void print_frames() const { return flxfrag->print_frames(); }

  // The number of words in the current event minus the header.
  size_t total_words() const { return flxfrag->total_words(); }
  // The number of frames in the current event.
  size_t total_frames() const { return flxfrag->total_frames(); }
  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const { return flxfrag->total_adc_values(); }

 private:
  const FelixFragmentBase* flxfrag;
}; // class dune::FelixFragment

#endif /* artdaq_dune_Overlays_FelixFragment_hh */
