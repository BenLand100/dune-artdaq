#ifndef FRAME_EXPAND_H
#define FRAME_EXPAND_H
#include <array>
#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "dune-artdaq/Generators/Felix/Types.hh"

#include "immintrin.h"

struct WindowCollectionADCs {
    WindowCollectionADCs(size_t numMessages_, MessageCollectionADCs* fragments_)
        : numMessages(numMessages_),
          fragments(fragments_)
    {}

    // Get a pointer to register `ireg` at time `itime`, as an AVX2 int register
    const __m256i* get256(size_t ireg, size_t itime) const
    {
            const size_t msg_index=itime/12;
            const size_t msg_time_offset=itime%12;
            const size_t index=msg_index*REGISTERS_PER_FRAME*FRAMES_PER_MSG+FRAMES_PER_MSG*ireg+msg_time_offset;
            const __m256i* rawp=reinterpret_cast<const __m256i*>(fragments)+index;
            return rawp;
    }

    uint16_t get16(size_t ichan, size_t itime) const
    {
        const size_t register_index=ichan/SAMPLES_PER_REGISTER;
        const size_t register_offset=ichan%SAMPLES_PER_REGISTER;
        const size_t register_t0_start=register_index*SAMPLES_PER_REGISTER*FRAMES_PER_MSG;
        const size_t msg_index=itime/12;
        const size_t msg_time_offset=itime%12;
        // The index in uint16_t of the start of the message we want
        const size_t msg_start_index=msg_index*sizeof(MessageCollectionADCs)/sizeof(uint16_t);
        const size_t offset_within_msg=register_t0_start+SAMPLES_PER_REGISTER*msg_time_offset+register_offset;
        const size_t index=msg_start_index+offset_within_msg;
        return *(reinterpret_cast<uint16_t*>(fragments)+index);
    }

    size_t numMessages;
    MessageCollectionADCs* fragments;
};

// A little wrapper around an array of 256-bit registers, so that we
// can explicitly access it as an array of 256-bit registers or as an
// array of uint16_t. (Also because directly creating a std::array of
// __m256i gives compiler warnings, presumably because the __m256i's
// aren't aligned)
template<int N>
class RegisterArray
{
public:
    // Get the value at the ith position as a 256-bit register
    __m256i ymm(size_t i) { return _mm256_lddqu_si256(reinterpret_cast<__m256i*>(m_array)+i); }
    void set_ymm(size_t i, __m256i val) { _mm256_storeu_si256(reinterpret_cast<__m256i*>(m_array)+i, val); }

    uint16_t uint16(size_t i) { return m_array[i]; }
    void set_uint16(size_t i, uint16_t val) { m_array[i]=val; }

    // Access the jth entry in the ith register
    uint16_t uint16(size_t i, size_t j) { return m_array[16*i+j]; }
    void set_uint16(size_t i, size_t j, uint16_t val) { m_array[16*i+j]=val; }

    uint16_t* data() { return m_array; }
    const uint16_t* data() const { return m_array; }
private:
    // alignas(32) std::array<uint16_t, N*16> m_array;
    uint16_t __restrict__ m_array[N*16];
};

//==============================================================================
// Print a 256-bit register interpreting it as packed 8-bit values
void print256(__m256i var);

//==============================================================================
// Print a 256-bit register interpreting it as packed 16-bit values
void print256_as16(__m256i var);

//==============================================================================
// Print a 256-bit register interpreting it as packed 16-bit values
void print256_as16_dec(__m256i var);

//==============================================================================
// Abortive attempt at expanding just the collection channels, instead
// of expanding all channels and then picking out just the collection
// ones. 
RegisterArray<2> expand_segment_collection(const dune::ColdataBlock& block);

//==============================================================================
// Take the raw memory containing 12-bit ADCs in the shuffled WIB
// format and rearrange them into 16-bit values in channel order. A
// 256-bit register holds 21-and-a-bit 12-bit values: we expand 16 of
// them into 16-bit values
__m256i expand_two_segments(const dune::ColdataSegment* first_segment);

//==============================================================================

// Get all the collection channel values from a dune::ColdataBlock as 16-bit
// values into 2 256-bit registers. Implemented by expanding all the
// values using expand_two_segments, and then picking out the
// collection channels with a blend. There are only 12 collection
// channels in a dune::ColdataBlock, so we shuffle valid values into the
// 0-11 entries of the register, and leave 4 invalid values at the end of each
// register
RegisterArray<2> get_block_collection_adcs(const dune::ColdataBlock& block);

//==============================================================================
// As above, for all collection and induction ADCs
RegisterArray<4> get_block_all_adcs(const dune::ColdataBlock& block);

//==============================================================================
// Expand all the collection channels into 6 AVX2 registers
RegisterArray<REGISTERS_PER_FRAME> get_frame_collection_adcs(const dune::FelixFrame* frame);

//==============================================================================
// As above, for all collection and induction ADCs
RegisterArray<16> get_frame_all_adcs(const dune::FelixFrame* frame);

//==============================================================================
int collection_index_to_offline(int index);

//======================================================================
RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expand_message_adcs(const SUPERCHUNK_CHAR_STRUCT& ucs);

#endif // include guard
