#ifndef NETIO_WIB_RECORDS_H
#define NETIO_WIB_RECORDS_H

#include <iostream>
#include <sstream>
#include <iomanip>
#include <bitset>
#include <utility>
#include <cstdlib>
#include <x86intrin.h>
#include <emmintrin.h>

#include "dune-raw-data/Overlays/FelixFormat.hh"

/* NetioWIBRecords
 * Author: Roland.Sipos@cern.ch
 * Description: WIB specific structures for NetioHandler
*/

// How many frames are concatenated in one netio message
static constexpr size_t FRAMES_PER_MSG=12;
// How many collection-wire AVX2 registers are returned per
// frame.
static constexpr size_t REGISTERS_PER_FRAME=6;
// How many bytes are in an AVX2 register
static constexpr size_t BYTES_PER_REGISTER=32;
// How many samples are in a register
static constexpr size_t SAMPLES_PER_REGISTER=16;

static const size_t NETIO_MSG_SIZE=sizeof(dune::FelixFrame)*FRAMES_PER_MSG;

// FELIX Header
struct FromFELIXHeader
{
  uint16_t length;
  uint16_t status;
  uint32_t elinkid : 6; 
  uint32_t gbtid : 26;
};
static_assert(sizeof(struct FromFELIXHeader) == 8, "Check your assumptions on FromFELIXHeader!");

// Buffer for Netio messages that are WIB specific.
const size_t WIB_FRAME_SIZE=464;
struct WIB_CHAR_STRUCT {
  char fragments[WIB_FRAME_SIZE];
};
static_assert(sizeof(struct WIB_CHAR_STRUCT) == 464, "Check your assumptions on WIB_CHAR_STRUCT!");

static const size_t IOVEC_FRAME_SIZE = 236640;
struct IOVEC_CHAR_STRUCT {
  char fragments[IOVEC_FRAME_SIZE];
};
static_assert(sizeof(struct IOVEC_CHAR_STRUCT) == 236640, "Check your assumptions on IOVEC_CHAR_STRUCT");

static const size_t SUPERCHUNK_FRAME_SIZE = 5568; // for 12: 5568  for 6: 2784
struct SUPERCHUNK_CHAR_STRUCT {
    char fragments[SUPERCHUNK_FRAME_SIZE];
};
static_assert(sizeof(struct SUPERCHUNK_CHAR_STRUCT) == 5568, "Check your assumptions on SUPERCHUNK_CHAR_STRUCT");

// One netio message's worth of collection channel ADCs after
// expansion: 12 frames per message times 8 registers per frame times
// 32 bytes (256 bits) per register
static const size_t collection_adcs_size=BYTES_PER_REGISTER*REGISTERS_PER_FRAME*FRAMES_PER_MSG;
struct MessageCollectionADCs {
    char fragments[collection_adcs_size];
};

/*
static void printHexChars(const WIB_CHAR_STRUCT& wct){
  //ostringstream oss;
  for(size_t i=0; i<WIB_FRAME_SIZE; ++i){
    if(i%32 == 0)
      {
        if(i != 0)
          std::cout << '\n';
        std::cout << "0x" << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
      }
      std::cout << std::hex << std::setfill('0') << std::setw(2) << ((unsigned int)wct.fragments[i] & 0xFF) << " " << std::dec;
  }
  std::cout << "\n\n";
  //return oss.str();
}
*/

/*
std::string printVecHex(const std::vector<uint8_t>& msgVec){
  std::ostringstream oss;
  auto data = std::string((const char*)msgVec.data(), msgVec.size());
  for(unsigned i = 0; i < data.size(); i++)
  {
    if(i%32 == 0)
    {
      if(i != 0)
        oss << '\n';
      oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
    }
    oss << std::hex << std::setfill('0') << std::setw(2) << ((unsigned int)data[i] & 0xFF) << " " << std::dec;
  }
  oss << "\n\n";
  return oss.str();
}



std::string printMsgHex(const netio::message& msg){
  std::ostringstream oss;
  auto data = std::string((const char*)msg.data_copy().data(), msg.size());
  for(unsigned i = 0; i < data.size(); i++)
  {
    if(i%32 == 0)
    {
      if(i != 0)
        oss << '\n';
      oss << "0x" << std::hex << std::setfill('0') << std::setw(8) << i << ": ";
    }
    oss << std::hex << std::setfill('0') << std::setw(2) << ((unsigned int)data[i] & 0xFF) << " " << std::dec;
  }
  oss << "\n\n";
  return oss.str();
}
*/

typedef uint_fast64_t t_field;
union AVX_FRAME {
  struct { 
    t_field f0, f1, f2, f3, f4, f5, f6, f7;
  } inner;
  __m256d v0;
  __m256d v1;
  AVX_FRAME(uint64_t t0, uint64_t t1, uint64_t t2, uint64_t t3,
            uint64_t t4, uint64_t t5, uint64_t t6, uint64_t t7)
  {
    v0 = _mm256_setr_pd(t3, t2, t1, t0);
    v1 = _mm256_setr_pd(t7, t6, t5, t4);
  }
};


// FELIX Fragment from Milo
typedef uint32_t word_t;
typedef uint_fast64_t t_field;

// PAR 2018-12-03: WIBHeader and ColdataHeader are both also defined in FelixFormat.hh, so try commenting them out here

// struct WIBHeader {
//   word_t sof : 8, version : 5, fiber_no : 3, crate_no : 5, slot_no : 3,
//       reserved_1 : 8;
//   word_t mm : 1, oos : 1, reserved_2 : 14, wib_errors : 16;
//   word_t timestamp_1;
//   word_t timestamp_2 : 16, wib_counter_1 : 15, z : 1;
//   uint64_t timestamp() const {
//     uint64_t timestamp = timestamp_1 | ((uint64_t)timestamp_2 << 32);
//     if (!z) { timestamp |= (uint64_t)wib_counter_1 << 48; }
//     return timestamp;
//   }
//   uint16_t wib_counter() const { return z ? wib_counter_1 : 0; }
//   void set_timestamp(const uint64_t new_timestamp) {
//     timestamp_1 = new_timestamp;
//     timestamp_2 = new_timestamp >> 32;
//     if (!z) { wib_counter_1 = new_timestamp >> 48; }
//   }
//   void set_wib_counter(const uint16_t new_wib_counter) {
//     if(z) { wib_counter_1 = new_wib_counter; }
//   }

// void print() const {
//     std::cout << "SOF:" << unsigned(sof) << " version:" << unsigned(version)
//               << " fiber:" << unsigned(fiber_no)
//               << " slot:" << unsigned(slot_no)
//               << " crate:" << unsigned(crate_no) << " mm:" << unsigned(mm)
//               << " oos:" << unsigned(oos)
//               << " wib_errors:" << unsigned(wib_errors)
//               << " timestamp: " << timestamp() << '\n';
//   }
//   void printHex() const {
//     std::cout << std::hex << "SOF:" << sof << " version:" << version
//               << " fiber:" << fiber_no << " slot:" << slot_no
//               << " crate:" << crate_no << " mm:" << mm << " oos:" << oos
//               << " wib_errors:" << wib_errors << " timestamp: " << timestamp()
//               << std::dec << '\n';
//   }
//   void printBits() const {
//     std::cout << "SOF:" << std::bitset<8>(sof)
//               << " version:" << std::bitset<5>(version)
//               << " fiber:" << std::bitset<3>(fiber_no)
//               << " slot:" << std::bitset<5>(slot_no)
//               << " crate:" << std::bitset<3>(crate_no) << " mm:" << bool(mm)
//               << " oos:" << bool(oos)
//               << " wib_errors:" << std::bitset<16>(wib_errors)
//               << " timestamp: " << timestamp() << '\n'
//               << " Z: " << z << '\n';
//   }
// };

// struct ColdataHeader {
//   word_t s1_error : 4, s2_error : 4, reserved_1 : 8, checksum_a_1 : 8,
//       checksum_b_1 : 8;
//   word_t checksum_a_2 : 8, checksum_b_2 : 8, coldata_convert_count : 16;
//   word_t error_register : 16, reserved_2 : 16;
//   word_t hdr_1 : 4, hdr_3 : 4, hdr_2 : 4, hdr_4 : 4, hdr_5 : 4, hdr_7 : 4,
//       hdr_6 : 4, hdr_8 : 4;

//   uint16_t checksum_a() const {
//     return (uint16_t)checksum_a_1 | (checksum_a_2 << 8);
//   }
//   uint16_t checksum_b() const {
//     return (uint16_t)checksum_b_1 | (checksum_b_2 << 8);
//   }
//   uint8_t hdr(const uint8_t i) const {
//     switch(i) {
//       case 1: return hdr_1;
//       case 2: return hdr_2;
//       case 3: return hdr_3;
//       case 4: return hdr_4;
//       case 5: return hdr_5;
//       case 6: return hdr_6;
//       case 7: return hdr_7;
//       case 8: return hdr_8;
//     }
//     return 0;
//   }

//   void set_checksum_a(const uint16_t new_checksum_a) {
//     checksum_a_1 = new_checksum_a;
//     checksum_a_2 = new_checksum_a >> 8;
//   }
//   void set_checksum_b(const uint16_t new_checksum_b) {
//     checksum_b_1 = new_checksum_b;
//     checksum_b_2 = new_checksum_b >> 8;
//   }
//   void set_hdr(const uint8_t i, const uint8_t new_hdr) {
//     switch(i) {
//       case 1: hdr_1 = new_hdr; break;
//       case 2: hdr_2 = new_hdr; break;
//       case 3: hdr_3 = new_hdr; break;
//       case 4: hdr_4 = new_hdr; break;
//       case 5: hdr_5 = new_hdr; break;
//       case 6: hdr_6 = new_hdr; break;
//       case 7: hdr_7 = new_hdr; break;
//       case 8: hdr_8 = new_hdr; break;
//     }
//   }

// void print() const {
//     std::cout << "s1_error:" << unsigned(s1_error)
//               << " s2_error:" << unsigned(s2_error)
//               << " checksum_a1:" << unsigned(checksum_a_1)
//               << " checksum_b1:" << unsigned(checksum_b_1)
//               << " checksum_a2:" << unsigned(checksum_a_2)
//               << " checksum_b1:" << unsigned(checksum_b_2)
//               << " coldata_convert_count:" << unsigned(coldata_convert_count)
//               << " error_register:" << unsigned(error_register)
//               << " hdr_1:" << unsigned(hdr_1) << " hdr_2:" << unsigned(hdr_2)
//               << " hdr_3:" << unsigned(hdr_3) << " hdr_4:" << unsigned(hdr_4)
//               << " hdr_5:" << unsigned(hdr_5) << " hdr_6:" << unsigned(hdr_6)
//               << " hdr_7:" << unsigned(hdr_7) << " hdr_8:" << unsigned(hdr_8);
//     std::cout << '\n';
//   }
//   void printHex() const {
//     std::cout << std::hex << "s1_error:" << s1_error << " s2_error:" << s2_error
//               << " checksum_a1:" << checksum_a_1
//               << " checksum_b1:" << checksum_b_1
//               << " checksum_a2:" << checksum_a_2
//               << " checksum_b1:" << checksum_b_2
//               << " coldata_convert_count:" << coldata_convert_count
//               << " error_register:" << error_register << " hdr_1:" << hdr_1
//               << " hdr_2:" << hdr_2 << " hdr_3:" << hdr_3 << " hdr_4:" << hdr_4
//               << " hdr_5:" << hdr_5 << " hdr_6:" << hdr_6 << " hdr_7:" << hdr_7
//               << " hdr_8:" << hdr_8;
//     std::cout << '\n';
//   }
//   void printBits() const {
//     std::cout << "s1_error:" << std::bitset<4>(s1_error)
//               << " s2_error:" << std::bitset<4>(s2_error)
//               << " checksum_a1:" << std::bitset<8>(checksum_a_1)
//               << " checksum_b1:" << std::bitset<8>(checksum_b_1)
//               << " checksum_a2:" << std::bitset<8>(checksum_a_2)
//               << " checksum_b2:" << std::bitset<8>(checksum_b_2)
//               << " coldata_convert_count:"
//               << std::bitset<16>(coldata_convert_count)
//               << " error_register:" << std::bitset<16>(error_register)
//               << " hdr_1:" << std::bitset<8>(hdr_1)
//               << " hdr_2:" << std::bitset<8>(hdr_2)
//               << " hdr_3:" << std::bitset<8>(hdr_3)
//               << " hdr_4:" << std::bitset<8>(hdr_4)
//               << " hdr_5:" << std::bitset<8>(hdr_5)
//               << " hdr_6:" << std::bitset<8>(hdr_6)
//               << " hdr_7:" << std::bitset<8>(hdr_7)
//               << " hdr_8:" << std::bitset<8>(hdr_8);
//     std::cout << '\n';
//   }
// };

#endif //  NETIO_WIB_RECORDS_H

