// FelixFragment.hh edited from ToyFragment.hh by Milo Vermeulen and Roland
// Sipos (2017) to accept WIB frames.

#ifndef artdaq_dune_Overlays_FelixFragment_hh
#define artdaq_dune_Overlays_FelixFragment_hh

// Uncomment to use the bit field ADC access method.
// MAV 30-10-2017: The bit field ADC access method is ~twice as fast and
// provides the same results as the alternative.
#define BITFIELD_METHOD

#include "FragmentType.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFormat.hh"

#include <bitset>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "zlib.h"

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
class FelixFragment;

// Bit access function (from FrameGen).
inline uint32_t get32BitRange(const uint32_t& word, const uint8_t& begin,
                              const uint8_t& end) {
  return (word >> begin) & ((1 << (end - begin + 1)) - 1);
}
}  // namespace dune

//============================
// FELIX fragment base class
//============================
class dune::FelixFragmentBase {
 public:
  /* Struct to hold FelixFragment Metadata. */
  struct Metadata {
    typedef uint32_t data_t;

    data_t num_frames : 16;
    data_t reordered : 8;
    data_t compressed : 8;

    static size_t const size_words = 1ul; /* Units of Metadata::data_t */
  };

  static_assert(sizeof(Metadata) ==
                    Metadata::size_words * sizeof(Metadata::data_t),
                "FelixFragment::Metadata size changed");

  /* Static file reader for debugging purpose. */
  static artdaq::FragmentPtr fromFile(const std::string& fileName) {
    std::ifstream in(fileName, std::ios::binary);
    std::string contents((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());
    Metadata meta;
    static size_t fragment_ID = 1;
    std::unique_ptr<artdaq::Fragment> frag_ptr(artdaq::Fragment::FragmentBytes(
        contents.size(), 1, fragment_ID, dune::toFragmentType("FELIX"), meta));
    ++fragment_ID;

    frag_ptr->resizeBytes(contents.size());
    memcpy(frag_ptr->dataBeginBytes(), contents.c_str(), contents.size());
    in.close();
    return frag_ptr;
  }

  /* FELIX-specific metadata from the FelixBoardReader included here for
   * debugging. */
  struct FelixHeader;

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
    // // Reset metadata number of frames if the data isn't compressed.
    // if(!meta_.compressed && meta_.reordered) {
    //   std::cout << "Changing number of frames from " << meta_.num_frames
    //             << " to ";
    //   meta_.num_frames = sizeBytes_ / (sizeof(dune::WIBHeader) +
    //                                    4 * sizeof(dune::ColdataHeader) +
    //                                    256 * sizeof(dune::adc_t));
    //   std::cout << meta_.num_frames << '\n';
    // } else if (!meta_.compressed && !meta_.reordered) {
    //   std::cout << "Changing number of frames from " << meta_.num_frames << " to ";
    //   meta_.num_frames = sizeBytes_ / sizeof(dune::FelixFrame);
    //   std::cout << meta_.num_frames << '\n';
    // }
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
// FELIX fragment for an array of bare FELIX frames
//==================================================
class dune::FelixFragmentUnordered : public dune::FelixFragmentBase {
 public:
  /* FELIX-specific metadata from the FelixBoardReader included here for
   * debugging. */
  struct FelixHeader {
    word_t flx_head;  // 4byte: 00 00 cd ab
  };

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
    return meta_.num_frames;
  }
  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const {
    return total_frames() * FelixFrame::num_ch_per_frame;
  }

 protected:
  // Allow access to individual frames according to the FelixFrame structure.
  FelixFrame const* frame_(const unsigned& frame_num = 0) const {
    return static_cast<dune::FelixFrame const*>(artdaq_Fragment_) + frame_num;
  }
}; // class dune::FelixFragmentUnordered

//=======================================================
// FELIX fragment for an array of reordered FELIX frames
//=======================================================
class dune::FelixFragmentReordered : public dune::FelixFragmentBase {
 public:
  /* FELIX-specific metadata from the FelixBoardReader included here for
   * debugging. */
  struct FelixHeader {};

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
      : FelixFragmentBase(fragment), bad_header_num(total_frames(), 0) {
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
  size_t total_frames() const { return meta_.num_frames; }

  // The number of ADC values describing data beyond the header
  size_t total_adc_values() const {
    return total_frames() * FelixFrame::num_ch_per_frame;
  }

 protected:
  // Important positions within the data buffer.
  const unsigned int adc_start = 0;
  const unsigned int bitlist_start = adc_start + total_frames() * 256 * sizeof(adc_t);
  const unsigned int header_start = bitlist_start + (total_frames()+7)/8;
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
             frame_num + ch_num * total_frames());
  }
}; // class dune::FelixFragmentReordered

//=======================================
// FELIX fragment for compressed frames.
//=======================================
class dune::FelixFragmentCompressed : public FelixFragmentBase {
 private:
  int decompress_copy(const artdaq::Fragment& src, artdaq::Fragment& dest) {
    // Determine and reserve new fragment size.
    long unsigned int uncompSizeBytes;
    const Metadata meta = *src.metadata<Metadata>();
    if(meta.reordered) {
      uncompSizeBytes = meta.num_frames * (sizeof(dune::WIBHeader) +
                                       4 * sizeof(dune::ColdataHeader) +
                                       256 * sizeof(dune::adc_t));
    } else {
      uncompSizeBytes = meta.num_frames * sizeof(dune::FelixFrame);
    }
    dest.resizeBytes(uncompSizeBytes);
    std::cout << "Number of frames in metadata: " << meta.num_frames << '\n';
    std::cout << "Calculated new size: " << uncompSizeBytes << '\n';

    // uncompress(dest.dataBeginBytes(), &uncompSizeBytes, src.dataBeginBytes(), src.dataSizeBytes());

    // // Zlib decompression.
    // // Overly long initialisation to avoid compiler warnings.
    // z_stream zInfo = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // zInfo.total_in =  zInfo.avail_in = src.dataSizeBytes();
    // zInfo.total_out = zInfo.avail_out = dest.dataSizeBytes();
    // zInfo.next_in = (uint8_t*)src.dataBeginBytes();
    // zInfo.next_out = dest.dataBeginBytes();

    // int nErr, nRet = -1;
    // // Extra MAX_WBITS | 16 argument to set gzip decompression.
    // nErr = inflateInit2(&zInfo, MAX_WBITS | 16);
    // if ( nErr == Z_OK ) {
    //   // zInfo.avail_in = CHUNK;
    //   // if(zInfo.total_in > )
    //   // zInfo.avail_out = CHUNK;
    //   nErr = inflate(&zInfo, Z_FINISH);
    //   if (nErr == Z_STREAM_END) {
    //     nRet = zInfo.total_out;
    //   }
    // }
    // inflateEnd( &zInfo );
    // return( nRet ); // -1 or len of output

    #define CHUNK 65536

    int ret;
    z_stream strm;
    // unsigned char in[CHUNK];
    // unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit2(&strm, MAX_WBITS | 16);
    if (ret != Z_OK) return ret;

    /* decompress until deflate stream ends or end of file */
    do {
      // Determine whether there is still data to decompress.
      if(strm.total_in + CHUNK < src.dataSizeBytes()) {
        strm.avail_in = CHUNK;
      } else {
        strm.avail_in = src.dataSizeBytes() - strm.total_in;
      }
      // return Z_ERRNO;
      if (strm.avail_in == 0) break;
      // memcpy(in, src.dataBeginBytes() + strm.total_in, strm.avail_in);
      strm.next_in = (Bytef*)(src.dataBeginBytes() + strm.total_in);
      // strm.total_in += strm.avail_in;

      /* run inflate() on input until output buffer not full */
      do {
        static unsigned iterations = 0;
        std::cout << "Inner: " << iterations++ << '\n';
        if(strm.total_out + CHUNK < dest.dataSizeBytes()) {
          strm.avail_out = CHUNK;
        } else {
          strm.avail_out = dest.dataSizeBytes() - strm.total_out;
        }
        std::cout << "strm.avail_in: " << strm.avail_in << '\n';
        std::cout << "strm.total_in: " << strm.total_in << '\n';
        std::cout << "strm.avail_out: " << strm.avail_out << '\n';
        std::cout << "strm.total_out: " << strm.total_out << '\n';
        strm.next_out = dest.dataBeginBytes() + strm.total_out;
        ret = inflate(&strm, Z_NO_FLUSH);
        strm.total_out += strm.avail_out;
        std::cout << " new strm.avail_in: " << strm.avail_in << '\n';
        std::cout << " new strm.total_in: " << strm.total_in << '\n';
        std::cout << " new strm.avail_out: " << strm.avail_out << '\n';
        std::cout << " new strm.total_out: " << strm.total_out << '\n';
      } while (strm.avail_out == 0);
      static unsigned outeriterations = 0;
      std::cout << "Outer: " << outeriterations++ << '\n';

      /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return ret == Z_STREAM_END
               ? Z_OK
               : Z_DATA_ERROR;

    // memcpy(dest.dataBeginBytes(), src.dataBeginBytes(), src.dataSizeBytes());
  }

 public:
  FelixFragmentCompressed(const artdaq::Fragment& fragment) : FelixFragmentBase(fragment) {
    // Decompress.
    std::cout << decompress_copy(fragment, uncompfrag_) << " returned from decompress_copy\n";

    // Handle metadata.
    Metadata meta = *fragment.metadata<Metadata>();
    meta.compressed = 0;
    uncompfrag_.setMetadata(meta);
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
    if(meta_.compressed) {
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
