#include "frame_expand.h"

//==============================================================================
// Print a 256-bit register interpreting it as packed 8-bit values
void print256(__m256i var)
{
    uint8_t *val = (uint8_t*) &var;
    for(int i=0; i<32; ++i) printf("%02x ", val[i]);
}

//==============================================================================
// Print a 256-bit register interpreting it as packed 16-bit values
void print256_as16(__m256i var)
{
    uint16_t *val = (uint16_t*) &var;
    for(int i=0; i<16; ++i) printf("%04x ", val[i]);
}

//==============================================================================
// Print a 256-bit register interpreting it as packed 16-bit values
void print256_as16_dec(__m256i var)
{
    int16_t *val = (int16_t*) &var;
    for(int i=0; i<16; ++i) printf("%+6d ", val[i]);
}

//==============================================================================
// Abortive attempt at expanding just the collection channels, instead
// of expanding all channels and then picking out just the collection
// ones.
RegisterArray<2> expand_segment_collection(const dune::ColdataBlock& __restrict__ block)
{
    const __m256i* __restrict__ coldata_start=reinterpret_cast<const __m256i*>(&block.segments[0]);
    __m256i raw0=_mm256_lddqu_si256(coldata_start+0);
    __m256i raw1=_mm256_lddqu_si256(coldata_start+1);
    __m256i raw2=_mm256_lddqu_si256(coldata_start+2);

    // For the first output item, we want these bytes:
    //
    // From raw0: 7, 9, 11, 13, 15, 17, 19, 21, 23, 24, 26, 28, 30
    // From raw1: 0, 2, 4, 6, 8
    //
    // They're non-overlapping so we can blend them
    //
    // So we get a blend mask of (where "x" means "don't care"):
    //
    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
    // -------------------------------------------------------------------------------------
    //  x  0  x  0  x  0  x  0  0  x  0  x  0  x  0  x  0  x  0  x  0  x 0 1 0 1 x 1 x 1 x 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    const __m256i blend_mask0=_mm256_set_epi8(0xdd, 0x00, 0xdd, 0x00, 0xdd, 0x00, 0xdd, 0x00,
                                              0x00, 0xdd, 0x00, 0xdd, 0x00, 0xdd, 0x00, 0xdd,
                                              0x00, 0xdd, 0x00, 0xdd, 0x00, 0xdd, 0x00, 0xff,
                                              0x00, 0xff, 0xdd, 0xff, 0xdd, 0xff, 0xdd, 0xff);
    // For the second output item, we want these bytes:
    //
    // From raw1: 23, 25, 27, 29, 31
    // From raw2: 1, 3, 5, 7, 8, 10, 12, 14, 16, 18, 20, 22, 24
    //
    // They're non-overlapping so we can blend them
    //
    // So we get a blend mask of (where "x" means "don't care"):
    //
    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
    // -------------------------------------------------------------------------------------
    //  0  x  0  x  0  x  0  1  0  1  x  1  x  1  x  1  x  1  x  1  x  1 x 1 1 x 1 x 1 x 1 x
    const __m256i blend_mask1=_mm256_set_epi8(0x00, 0xdd, 0x00, 0xdd, 0x00, 0xdd, 0x00, 0xff,
                                              0x00, 0xff, 0xdd, 0xff, 0xdd, 0xff, 0xdd, 0xff,
                                              0xdd, 0xff, 0xdd, 0xff, 0xdd, 0xff, 0xdd, 0xff,
                                              0xff, 0xdd, 0xff, 0xdd, 0xff, 0xdd, 0xff, 0xdd);
#pragma GCC diagnostic pop
    __m256i out0=_mm256_blendv_epi8(raw0, raw1, blend_mask0);
    __m256i out1=_mm256_blendv_epi8(raw1, raw2, blend_mask1);
    print256(out0); printf("  out0\n");
    print256(out1); printf("  out1\n");

    // ...then I got bored trying to work out the shuffle needed, but
    // it's possible in principle
    return RegisterArray<2>();
}

//==============================================================================
// Take the raw memory containing 12-bit ADCs in the shuffled WIB
// format and rearrange them into 16-bit values in channel order. A
// 256-bit register holds 21-and-a-bit 12-bit values: we expand 16 of
// them into 16-bit values
__m256i expand_two_segments(const dune::ColdataSegment* __restrict__ first_segment)
{
    const __m256i* __restrict__ segments_start=reinterpret_cast<const __m256i*>(first_segment);
    __m256i raw=_mm256_lddqu_si256(segments_start);

    // First: the ADCs are arranged in a repeating pattern of 3 32-bit
    // words. The instructions we use later act won't shuffle items
    // across 128-bit lanes, so first we move the second block of 3
    // 32-bit words so it starts on the 128-bit boundary. Then we can
    // expand both blocks by doing the same thing on each 128-bit lane

    // The items at indices 3 and 7 are arbitrary: we won't be using
    // their values later
    const __m256i lane_shuffle=_mm256_setr_epi32(0,1,2,0,3,4,5,0);
    raw=_mm256_permutevar8x32_epi32(raw, lane_shuffle);

    // We eventually want to end up with one vector containing all the
    // low-order nibbles with zeros elsewhere, and the other
    // containing all the high-order nibbles with zeros elsewhere. The
    // smallest unit in which we can fetch items is 8 bits, so we
    // fetch 8-bit units, shuffle them and mask
    const __m256i shuffle_mask_lo=_mm256_setr_epi8(0, 2,    2,    4, 6,    8,  8,   10, 1,    3, 3,    5, 7, 9, 9, 11,
                                                   0, 2,    2,    4, 6,    8,  8,   10, 1,    3, 3,    5, 7, 9, 9, 11);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    const __m256i shuffle_mask_hi=_mm256_setr_epi8(0, 0xff, 4, 0xff, 6, 0xff, 10, 0xff, 1, 0xff, 5, 0xff, 7, 0xff, 11, 0xff,
                                                   0, 0xff, 4, 0xff, 6, 0xff, 10, 0xff, 1, 0xff, 5, 0xff, 7, 0xff, 11, 0xff);
#pragma GCC diagnostic pop
    // Get the 8-bit units into the 256-bit register in roughly the places they'll end up
    __m256i lo=_mm256_shuffle_epi8(raw, shuffle_mask_lo);
    __m256i hi=_mm256_shuffle_epi8(raw, shuffle_mask_hi);
    // Some nibbles in `lo` are in the high-order place when they
    // should be in the low-order place, and vice versa in `hi`. So we
    // make copies of the vectors that are shifted by one nibble, and
    // then use `blend` to select the shifted or unshifted version of
    // each item.
    __m256i lo_shift=_mm256_srli_epi16(lo, 4);
    __m256i hi_shift=_mm256_slli_epi16(hi, 4);
    // Whether we want the shifted or unshifted version alternates, so
    // the appropriate blend mask is 0xaa
    __m256i lo_blended=_mm256_blend_epi16(lo, lo_shift, 0xaa);
    __m256i hi_blended=_mm256_blend_epi16(hi, hi_shift, 0xaa);
    // Now we mask out the high-order nibbles in `lo`, and the low-order nibbles in `hi`, so that we can OR them together.
    // Effectively: (hi & 0xf0) | (lo & 0x0f)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    __m256i lo_nibble_mask=_mm256_set1_epi16(0x0f0f);
    __m256i hi_nibble_mask=_mm256_set1_epi16(0xf0f0);
#pragma GCC diagnostic pop
    __m256i lo_masked=_mm256_and_si256(lo_blended, lo_nibble_mask);
    __m256i hi_masked=_mm256_and_si256(hi_blended, hi_nibble_mask);
    __m256i final=_mm256_or_si256(lo_masked, hi_masked);
    // The low 128 bits of `final` contain 8 16-bit adc values
    return final;
}

//==============================================================================

// Get all the collection channel values from a dune::ColdataBlock as 16-bit
// values into 2 256-bit registers. Implemented by expanding all the
// values using expand_two_segments, and then picking out the
// collection channels with a blend. There are only 12 collection
// channels in a dune::ColdataBlock, so we shuffle valid values into the
// 0-11 entries of the register, and leave 4 invalid values at the end of each
// register
RegisterArray<2> get_block_collection_adcs(const dune::ColdataBlock& __restrict__ block)
{
    // First expand all of the channels into `expanded_all`
    __m256i expanded_all[4];
    for(int j=0; j<4; ++j){
        expanded_all[j]=expand_two_segments(&block.segments[2*j]);
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverflow"
    // Now select just the collection channels, using a blend
    __m256i blend_mask=_mm256_setr_epi16(0xffff, 0xffff, 0xffff, 0xffff, // 4 from the second segment, please Carol
                                         0x0, 0x0,                       // 2 don't care
                                         0x0, 0x0,                       // 2 from the first
                                         0xffff, 0xffff,                 // 2 from the second
                                         0x0, 0x0,                       // 2 don't care
                                         0x0, 0x0, 0x0, 0x0);            // 4 from the first
#pragma GCC diagnostic pop
    RegisterArray<2> expanded_coll;
    // Blend, and shuffle so that the "don't care"s are on the end
    const __m256i shuffle_mask=_mm256_setr_epi32(0, 1, 3, 4, 6, 7, 0, 0);
    expanded_coll.set_ymm(0, _mm256_permutevar8x32_epi32(_mm256_blendv_epi8(expanded_all[0], expanded_all[1], blend_mask),
                                                         shuffle_mask));
    expanded_coll.set_ymm(1, _mm256_permutevar8x32_epi32(_mm256_blendv_epi8(expanded_all[2], expanded_all[3], blend_mask),
                                                         shuffle_mask));

    // Set the dummy values to zero so they stand out more easily later on
    const int dummy_blend_mask=0xc0;
    const __m256i zero=_mm256_setzero_si256();
    expanded_coll.set_ymm(0, _mm256_blend_epi32(expanded_coll.ymm(0), zero, dummy_blend_mask));
    expanded_coll.set_ymm(1, _mm256_blend_epi32(expanded_coll.ymm(1), zero, dummy_blend_mask));

    return expanded_coll;
}

//==============================================================================
RegisterArray<4> get_block_all_adcs(const dune::ColdataBlock& __restrict__ block)
{
    RegisterArray<4> expanded_all;
    for(int j=0; j<4; ++j){
        expanded_all.set_ymm(j, expand_two_segments(&block.segments[2*j]));
    }
    return expanded_all;
}

//==============================================================================
//
RegisterArray<REGISTERS_PER_FRAME> get_frame_collection_adcs(const dune::FelixFrame* __restrict__ frame)
{
    // Each coldata block has 24 collection channels, so we have to
    // put it in two registers, using 12 of the 16 slots in each
    RegisterArray<8> adcs_tmp;
    for(int i=0; i<4; ++i){
        RegisterArray<2> block_adcs=get_block_collection_adcs(frame->block(i));
        adcs_tmp.set_ymm(2*i, block_adcs.ymm(0));
        adcs_tmp.set_ymm(2*i+1, block_adcs.ymm(1));
    }

    // Now adcs_tmp contains 96 values in 8 registers, but we can fit
    // those values in 6 registers, so do that. This way, the
    // downstream processing code has less to do.

    // We'll take advantage of the fact that there are 4 "null" values
    // (ie 4*16 bits = 64 bits) in the registers at the end, so we can
    // just treat the registers as containing 64-bit items.

    // We're going to move values from the last two registers (indices
    // 6 and 7) into the spaces in registers 0-5, so we first shuffle
    // register 6 so it has the items we want in the right place to
    // blend, and then blend
    RegisterArray<REGISTERS_PER_FRAME> adcs;

    // A register where every 64-bit word is the 0th 64-bit word from the original register
    __m256i reg0_quad0=_mm256_permute4x64_epi64(adcs_tmp.ymm(6), 0x00);
    // A register where every 64-bit word is the 1st 64-bit word from the original register
    __m256i reg0_quad1=_mm256_permute4x64_epi64(adcs_tmp.ymm(6), 0x55);
    // A register where every 64-bit word is the 2nd 64-bit word from the original register
    __m256i reg0_quad2=_mm256_permute4x64_epi64(adcs_tmp.ymm(6), 0xaa);

    // Likewise for register 7
    __m256i reg1_quad0=_mm256_permute4x64_epi64(adcs_tmp.ymm(7), 0x00);
    __m256i reg1_quad1=_mm256_permute4x64_epi64(adcs_tmp.ymm(7), 0x55);
    __m256i reg1_quad2=_mm256_permute4x64_epi64(adcs_tmp.ymm(7), 0xaa);

    // Treating the register as containing 8 32-bit items, this blend
    // mask takes 6 from the first register and 2 from the second
    // register, which is what we want
    int blend_mask=0xc0;

    adcs.set_ymm(0, _mm256_blend_epi32(adcs_tmp.ymm(0), reg0_quad0, blend_mask));
    adcs.set_ymm(1, _mm256_blend_epi32(adcs_tmp.ymm(1), reg0_quad1, blend_mask));
    adcs.set_ymm(2, _mm256_blend_epi32(adcs_tmp.ymm(2), reg0_quad2, blend_mask));

    adcs.set_ymm(3, _mm256_blend_epi32(adcs_tmp.ymm(3), reg1_quad0, blend_mask));
    adcs.set_ymm(4, _mm256_blend_epi32(adcs_tmp.ymm(4), reg1_quad1, blend_mask));
    adcs.set_ymm(5, _mm256_blend_epi32(adcs_tmp.ymm(5), reg1_quad2, blend_mask));

    return adcs;
}

//==============================================================================
RegisterArray<16> get_frame_all_adcs(const dune::FelixFrame* __restrict__ frame)
{
    RegisterArray<16> adcs;
    for(int i=0; i<4; ++i){
        RegisterArray<4> block_adcs=get_block_all_adcs(frame->block(i));
        adcs.set_ymm(4*i, block_adcs.ymm(0));
        adcs.set_ymm(4*i+1, block_adcs.ymm(1));
        adcs.set_ymm(4*i+2, block_adcs.ymm(2));
        adcs.set_ymm(4*i+3, block_adcs.ymm(3));
    }
    return adcs;
}

// This array used to be inside collection_index_to_offline(), but
// that caused the function to show up in the profile, so it's moved
// here to make sure it's really a compile-time constant
namespace {
    const int offlines[96]={
         12,   14,   16,   18,   23,   21,   20,   22,   19,   17,   15,   13,  264,  266,  268,  270, 
          0,    2,    4,    6,   11,    9,    8,   10,    7,    5,    3,    1,  275,  273,  272,  274, 
         24,   26,   28,   30,   35,   33,   32,   34,   31,   29,   27,   25,  271,  269,  267,  265, 
         36,   38,   40,   42,   47,   45,   44,   46,   43,   41,   39,   37,  276,  278,  280,  282, 
        252,  254,  256,  258,  263,  261,  260,  262,  259,  257,  255,  253,  287,  285,  284,  286, 
        240,  242,  244,  246,  251,  249,  248,  250,  247,  245,  243,  241,  283,  281,  279,  277
    };
}

//==============================================================================
int collection_index_to_offline(int index)
{
    // The offline channel number for each item in the
    // RegisterArray<6> returned by get_frame_collection_adcs(),
    // Values are given relative to the smallest collection wire
    // channel in the frame. There's some extra pattern here that I'm
    // too lazy to work out. This list was made by number_collection.cpp

    if(index<0 || index>95)  return -1;
    else                     return offlines[index];
}

//======================================================================
RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expand_message_adcs(const SUPERCHUNK_CHAR_STRUCT& __restrict__ ucs)
{
    RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> adcs;
    for(size_t iframe=0; iframe<FRAMES_PER_MSG; ++iframe){
        const dune::FelixFrame* frame=reinterpret_cast<const dune::FelixFrame*>(&ucs) + iframe;
        RegisterArray<REGISTERS_PER_FRAME> collection_adcs=get_frame_collection_adcs(frame);
        for(size_t iblock=0; iblock<REGISTERS_PER_FRAME; ++iblock){
            // Arrange it so that adjacent times are adjacent in
            // memory, which will hopefully make the trigger primitive
            // finding code itself a little easier
            //
            // So the memory now looks like:
            // (register 0, time 0) (register 0, time 1) ... (register 0, time 11)
            // (register 1, time 0) (register 1, time 1) ... (register 1, time 11)
            // ...
            // (register 5, time 0) (register 5, time 1) ... (register 5, time 11)
            adcs.set_ymm(iframe+iblock*FRAMES_PER_MSG, collection_adcs.ymm(iblock));
        }
    }
    return adcs;
}
