#ifndef FELIX_REORDERER_HH_
#define FELIX_REORDERER_HH_

/*
 * FelixReorderer
 * Authors
 *  Milo Vermeulen for base implementation
 *  Thijs Miedema for AVX implementation
 * Description: Reorder Felix Frames in software to group headers and channeldata.
 * Date: July 2018
*/

#include <inttypes.h>
#include <immintrin.h>
#include <string>
#include <iomanip>
#include <cstring>


class FelixReorderer
{

  private:
    /// SIZE CONSTANTS ///
    static constexpr size_t num_blocks_per_frame = 4;
    static constexpr size_t num_ch_per_frame = 256;
    static constexpr size_t num_ch_per_block = 64;
    static constexpr size_t num_seg_per_block = 8;
    static constexpr size_t num_ch_per_seg = 8;

    static constexpr size_t num_bytes_per_seg = 12;
    static constexpr size_t num_bytes_per_reord_seg = 16;
    static constexpr size_t num_bytes_per_block = num_bytes_per_seg * num_seg_per_block;
    static constexpr size_t num_uint16_per_reord_seg = 8;
    static constexpr size_t coldata_header_size = 4 * 4;
    static constexpr size_t wib_header_size = 4 * 4;
    static constexpr size_t num_bytes_per_data = num_ch_per_frame * 2;
    static constexpr size_t adc_size = 2;

    /// BIT OFFSET CONSTANTS ///
    // Segments //
    static const unsigned b_seg_0 =  0;
    static const unsigned b_seg_1 = 12;
    static const unsigned b_seg_2 = 24;
    static const unsigned b_seg_3 = 36;

    // ADC value byte positions
    static const uint8_t b_adc0_ch0_p0 = 0;
    static const uint8_t b_adc0_ch0_p1 = 2;
    static const uint8_t b_adc0_ch1_p0 = 2;
    static const uint8_t b_adc0_ch1_p1 = 4;
    static const uint8_t b_adc0_ch2_p0 = 6;
    static const uint8_t b_adc0_ch2_p1 = 8;
    static const uint8_t b_adc0_ch3_p0 = 8;
    static const uint8_t b_adc0_ch3_p1 = 10;
    static const uint8_t b_adc1_ch0_p0 = 1;
    static const uint8_t b_adc1_ch0_p1 = 3;
    static const uint8_t b_adc1_ch1_p0 = 3;
    static const uint8_t b_adc1_ch1_p1 = 5;
    static const uint8_t b_adc1_ch2_p0 = 7;
    static const uint8_t b_adc1_ch2_p1 = 9;
    static const uint8_t b_adc1_ch3_p0 = 9;
    static const uint8_t b_adc1_ch3_p1 = 11;

  public:
    /// Framesize public constants
    static constexpr size_t num_bytes_per_frame = wib_header_size + num_blocks_per_frame * (coldata_header_size + num_bytes_per_block);
    static constexpr size_t num_bytes_per_reord_frame = wib_header_size + num_blocks_per_frame * (coldata_header_size + num_ch_per_block * 2);

    /// METHODS ///
    static bool do_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames=10000) noexcept;
    static bool do_avx_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames=10000) noexcept;
    static bool do_avx512_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames=10000) noexcept;

    static const bool avx_available = 
    #ifdef AVX 
      true;
    #else 
      false;
    #endif 

    static const bool avx512_available = 
    #ifdef AVX512
      true;
    #else 
      false;
    #endif 

  private:
    /// FRAME OFFSETS ///
    static constexpr unsigned frame0 = 0 * num_bytes_per_frame;
    static constexpr unsigned frame1 = 1 * num_bytes_per_frame;
    static constexpr unsigned frame2 = 2 * num_bytes_per_frame;
    static constexpr unsigned frame3 = 3 * num_bytes_per_frame;

    /// MILO REORDERING ///
    static void wib_header_copy(uint8_t* dst, const uint8_t* src, const unsigned &num_frames);
    static void coldata_header_copy(uint8_t* dst, const uint8_t* src, const unsigned &num_frames);
    static void adc_copy(uint8_t* dst, const uint8_t* src, const unsigned &num_frames);

    #ifdef AVX
    /// AVX REORDERING ///
    static void reorder_avx_handle_four_segments(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx_handle_block(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx_handle_frame(const uint8_t* src, uint8_t* dst, unsigned frame_num, const unsigned &num_frames); 
    #endif
    #ifdef AVX512
    /// AVX512 REORDERING /// 
    static void reorder_avx512_handle_four_frames_two_segments(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx512_handle_four_frames_one_block(const uint8_t* src, uint8_t* dst, const unsigned &num_frames); 
    static void reorder_avx512_handle_four_frames(const uint8_t* src, uint8_t* dst, unsigned frame_num, const unsigned &num_frames);
    #endif

};
#endif /* FELIX_REORDERER_HH_ */
