#ifndef FELIX_REORDER_HH_
#define FELIX_REORDER_HH_

/*
 * FelixReorder
 * Authors
 *  Milo Vermeulen for base implementation
 *  Thijs Miedema for AVX implementation
 * Description: Reorder Felix Frames in software to group headers and channeldata.
 * Date: July 2018
*/

#include "dune-raw-data/Overlays/FelixFragment.hh"

#include <inttypes.h>
#include <immintrin.h>
#include <string>
#include <iomanip>
#include <cstring>


class FelixReorder
{
  private:
    /// SIZE CONSTANTS ///
    static constexpr size_t m_num_blocks_per_frame = 4;
    static constexpr size_t m_num_ch_per_frame = 256;
    static constexpr size_t m_num_ch_per_block = 64;
    static constexpr size_t m_num_seg_per_block = 8;
    static constexpr size_t m_num_ch_per_seg = 8;

    static constexpr size_t m_num_bytes_per_seg = 12;
    static constexpr size_t m_num_bytes_per_reord_seg = 16;
    static constexpr size_t m_num_bytes_per_block = m_num_bytes_per_seg * m_num_seg_per_block;
    static constexpr size_t m_coldata_header_size = 4 * 4;
    static constexpr size_t m_wib_header_size = 4 * 4;
    static constexpr size_t m_num_bytes_per_data = m_num_ch_per_frame * 2;
    static constexpr size_t m_adc_size = 2;

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
    static constexpr size_t m_num_bytes_per_frame = m_wib_header_size + m_num_blocks_per_frame * (m_coldata_header_size + m_num_bytes_per_block);
    static constexpr size_t m_num_bytes_per_reord_frame = m_wib_header_size + m_num_blocks_per_frame * (m_coldata_header_size + m_num_ch_per_block * 2);

    /// METHODS ///
    static bool do_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames, unsigned *num_faulty) noexcept;
    static bool do_avx_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames, unsigned *num_faulty) noexcept;
    static bool do_avx512_reorder(uint8_t* dst, const uint8_t* src, const unsigned &num_frames, unsigned *num_faulty) noexcept;

    static bool do_reorder_part(uint8_t* dst, const uint8_t* src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept;
    static bool do_avx_reorder_part(uint8_t* dst, const uint8_t* src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept;
    static bool do_avx512_reorder_part(uint8_t* dst, const uint8_t* src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept;

    static unsigned calculate_reordered_size(unsigned num_frames, unsigned num_faulty)
    {
        return m_num_bytes_per_data * num_frames + 
            (m_wib_header_size + m_num_blocks_per_frame * m_coldata_header_size) * 
            (num_faulty + 1) +
            (num_frames + 7) / 8;
    }

#ifdef __AVX2__ 
    static const bool avx_available = true;
#else 
    static const bool avx_available = false;
#endif 

#ifdef __AVX512__REMOVE_ME_AFTER_GCC_PATCH
    static const bool avx512_available = true;
#else 
    static const bool avx512_available = false;
#endif 

  private:
    /// FRAME OFFSETS ///
    static constexpr unsigned m_frame0 = 0 * m_num_bytes_per_frame;
    static constexpr unsigned m_frame1 = 1 * m_num_bytes_per_frame;
    static constexpr unsigned m_frame2 = 2 * m_num_bytes_per_frame;
    static constexpr unsigned m_frame3 = 3 * m_num_bytes_per_frame;

    static void copy_headers(uint8_t* dst, const uint8_t* src);
    static void handle_headers(uint8_t* dst, const uint8_t* src, const unsigned frame_index, const unsigned &num_frames, unsigned* num_faulty);

    /// BASELINE REORDERING ///
    static void baseline_handle_frames(uint8_t* dst, const uint8_t* src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty);

    #ifdef __AVX2__
    /// AVX2 REORDERING ///
    static void reorder_avx_handle_four_segments(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx_handle_block(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx_handle_frame(const uint8_t* src, uint8_t* dst, unsigned frame_num, const unsigned &num_frames, unsigned *num_faulty); 
    #endif
    #ifdef __AVX512__REMOVE_ME_AFTER_GCC_PATCH
    /// AVX512 REORDERING /// 
    static void reorder_avx512_handle_four_frames_two_segments(const uint8_t* src, uint8_t* dst, const unsigned &num_frames);
    static void reorder_avx512_handle_four_frames_one_block(const uint8_t* src, uint8_t* dst, const unsigned &num_frames); 
    static void reorder_avx512_handle_four_frames(const uint8_t* src, uint8_t* dst, unsigned frame_num, const unsigned &num_frames, unsigned *num_faulty);
    #endif

};

#endif /* FELIX_REORDER_HH_ */

