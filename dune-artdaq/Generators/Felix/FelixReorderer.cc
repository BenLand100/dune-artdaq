/// Thijs Miedema, last edit 2018-07-23

#include "FelixFormat.hh"
#include "FelixReorderer.hh"

#define UNUSED(x) (void)(x)

void FelixReorderer::wib_header_copy(uint8_t *dst, const uint8_t *psrc, const unsigned frames_start, const unsigned frames_stop)
{
    // Store WIB-headers next to each other.
    const FelixFrame *src =
        reinterpret_cast<FelixFrame const *>(psrc);
    for (unsigned i = frames_start; i < frames_stop; ++i)
    {
        memcpy(dst + i * m_wib_header_size, src->wib_header(), m_wib_header_size);
        ++src;
    }
}

void FelixReorderer::coldata_header_copy(uint8_t *dst, const uint8_t *psrc, const unsigned frames_start, const unsigned frames_stop)
{
    // Store COLDATA headers next to each other.
    const FelixFrame *src =
        reinterpret_cast<FelixFrame const *>(psrc);
    for (unsigned i = frames_start; i < frames_stop; ++i)
    {
        for (unsigned j = 0; j < m_num_blocks_per_frame; ++j)
        {
            memcpy(dst + (i * m_num_blocks_per_frame + j) * m_coldata_header_size, src->coldata_header(j), m_coldata_header_size);
        }
        ++src;
    }
}

void FelixReorderer::adc_copy(uint8_t *dst, const uint8_t *psrc, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames)
{
    // Store all ADC values in uint16_t.
    const FelixFrame *src =
        reinterpret_cast<FelixFrame const *>(psrc);

    for (unsigned fr = frames_start; fr < frames_stop; ++fr)
    {
        for (unsigned ch = 0; ch < m_num_ch_per_frame; ++ch)
        {
            adc_t curr_val = (src + fr - frames_start)->channel(ch);
            memcpy(dst + (ch * num_frames + fr) * m_adc_size, &curr_val, m_adc_size);
        }
    }
}

bool FelixReorderer::do_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames) noexcept
{
    try
    {
        wib_header_copy(dst, src, 0, num_frames);
        coldata_header_copy(dst + m_wib_header_size * num_frames, src, 0, num_frames);
        adc_copy(dst + (m_wib_header_size + 4 * m_coldata_header_size) * num_frames, src, 0, num_frames, num_frames);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorderer::do_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames) noexcept
{
    try
    {
        wib_header_copy(dst, src, frames_start, frames_stop);
        coldata_header_copy(dst + m_wib_header_size * num_frames, src, frames_start, frames_stop);
        adc_copy(dst + (m_wib_header_size + 4 * m_coldata_header_size) * num_frames, src, frames_start, frames_stop, num_frames);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

#ifdef __AVX2__
void FelixReorderer::reorder_avx_handle_four_segments(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    /// Set up the two registers
    __m256i noshift = _mm256_set_epi8(src[b_seg_3 + b_adc1_ch2_p1], src[b_seg_3 + b_adc1_ch2_p0], src[b_seg_3 + b_adc1_ch0_p1], src[b_seg_3 + b_adc1_ch0_p0],
                                      src[b_seg_2 + b_adc1_ch2_p1], src[b_seg_2 + b_adc1_ch2_p0], src[b_seg_2 + b_adc1_ch0_p1], src[b_seg_2 + b_adc1_ch0_p0],
                                      src[b_seg_3 + b_adc0_ch2_p1], src[b_seg_3 + b_adc0_ch2_p0], src[b_seg_3 + b_adc0_ch0_p1], src[b_seg_3 + b_adc0_ch0_p0],
                                      src[b_seg_2 + b_adc0_ch2_p1], src[b_seg_2 + b_adc0_ch2_p0], src[b_seg_2 + b_adc0_ch0_p1], src[b_seg_2 + b_adc0_ch0_p0],

                                      src[b_seg_1 + b_adc1_ch2_p1], src[b_seg_1 + b_adc1_ch2_p0], src[b_seg_1 + b_adc1_ch0_p1], src[b_seg_1 + b_adc1_ch0_p0],
                                      src[b_seg_0 + b_adc1_ch2_p1], src[b_seg_0 + b_adc1_ch2_p0], src[b_seg_0 + b_adc1_ch0_p1], src[b_seg_0 + b_adc1_ch0_p0],
                                      src[b_seg_1 + b_adc0_ch2_p1], src[b_seg_1 + b_adc0_ch2_p0], src[b_seg_1 + b_adc0_ch0_p1], src[b_seg_1 + b_adc0_ch0_p0],
                                      src[b_seg_0 + b_adc0_ch2_p1], src[b_seg_0 + b_adc0_ch2_p0], src[b_seg_0 + b_adc0_ch0_p1], src[b_seg_0 + b_adc0_ch0_p0]);

    __m256i toshift = _mm256_set_epi8(src[b_seg_3 + b_adc1_ch3_p1], src[b_seg_3 + b_adc1_ch3_p0], src[b_seg_3 + b_adc1_ch1_p1], src[b_seg_3 + b_adc1_ch1_p0],
                                      src[b_seg_2 + b_adc1_ch3_p1], src[b_seg_2 + b_adc1_ch3_p0], src[b_seg_2 + b_adc1_ch1_p1], src[b_seg_2 + b_adc1_ch1_p0],
                                      src[b_seg_3 + b_adc0_ch3_p1], src[b_seg_3 + b_adc0_ch3_p0], src[b_seg_3 + b_adc0_ch1_p1], src[b_seg_3 + b_adc0_ch1_p0],
                                      src[b_seg_2 + b_adc0_ch3_p1], src[b_seg_2 + b_adc0_ch3_p0], src[b_seg_2 + b_adc0_ch1_p1], src[b_seg_2 + b_adc0_ch1_p0],

                                      src[b_seg_1 + b_adc1_ch3_p1], src[b_seg_1 + b_adc1_ch3_p0], src[b_seg_1 + b_adc1_ch1_p1], src[b_seg_1 + b_adc1_ch1_p0],
                                      src[b_seg_0 + b_adc1_ch3_p1], src[b_seg_0 + b_adc1_ch3_p0], src[b_seg_0 + b_adc1_ch1_p1], src[b_seg_0 + b_adc1_ch1_p0],
                                      src[b_seg_1 + b_adc0_ch3_p1], src[b_seg_1 + b_adc0_ch3_p0], src[b_seg_1 + b_adc0_ch1_p1], src[b_seg_1 + b_adc0_ch1_p0],
                                      src[b_seg_0 + b_adc0_ch3_p1], src[b_seg_0 + b_adc0_ch3_p0], src[b_seg_0 + b_adc0_ch1_p1], src[b_seg_0 + b_adc0_ch1_p0]);

    /// Shift 4 bits right, all bits align now
    __m256i afshift = _mm256_srai_epi16(toshift, 4);

    /// Mask upper four bits of each 16 bit part
    const __m256i mask = _mm256_set_epi16(0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0xfff);
    __m256i v1 = _mm256_and_si256(noshift, mask);
    __m256i v2 = _mm256_and_si256(afshift, mask);

    /// Memcopy out values
    uint16_t *v1adc = (uint16_t *)&v1;
    uint16_t *v2adc = (uint16_t *)&v2;

    for (uint8_t i = 0; i < 2 * m_num_ch_per_seg; i++)
    {
        memcpy(((uint16_t *)dst) + 2 * i * num_frames, v1adc + i, m_adc_size);
        memcpy(((uint16_t *)dst) + (2 * i + 1) * num_frames, v2adc + i, m_adc_size);
    }
}

void FelixReorderer::reorder_avx_handle_block(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    reorder_avx_handle_four_segments(src, dst, num_frames);
    reorder_avx_handle_four_segments(src + 4 * m_num_bytes_per_seg, dst + 4 * m_num_bytes_per_reord_seg * num_frames, num_frames);
}

void FelixReorderer::reorder_avx_handle_frame(const uint8_t *src, uint8_t *dst, unsigned frame_num, const unsigned &num_frames = 10000)
{
    /// Destinations
    uint8_t *wib_header_destination = dst + frame_num * m_wib_header_size;
    uint8_t *coldata_header_destination = dst + num_frames * m_wib_header_size + frame_num * m_coldata_header_size * m_num_blocks_per_frame;
    uint8_t *data_destination = dst + num_frames * (m_wib_header_size + m_coldata_header_size * m_num_blocks_per_frame) + frame_num * m_adc_size;

    /// Sources
    const uint8_t *coldata_start = src + m_wib_header_size;
    const uint8_t *data_start = coldata_start + m_coldata_header_size;

    /// Copies
    memcpy(wib_header_destination, src, m_wib_header_size);

    for (unsigned i = 0; i < m_num_blocks_per_frame; ++i)
    {
        memcpy(coldata_header_destination + i * (m_coldata_header_size), coldata_start + i * (m_coldata_header_size + m_num_bytes_per_block), m_coldata_header_size);
        reorder_avx_handle_block(data_start + i * (m_coldata_header_size + m_num_bytes_per_block),
                                 data_destination + i * m_num_ch_per_block * m_adc_size * num_frames, num_frames);
    }
}

bool FelixReorderer::do_avx_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames) noexcept
{
    try
    {
        for (unsigned i = 0; i < num_frames; i++)
        {
            reorder_avx_handle_frame(src + i * m_num_bytes_per_frame, dst, i, num_frames);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorderer::do_avx_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames) noexcept
{
    try
    {
        for (unsigned i = 0; i < frames_stop - frames_start; i++)
        {
            reorder_avx_handle_frame(src + i * m_num_bytes_per_frame, dst, frames_start + i, num_frames);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

#else
bool FelixReorderer::do_avx_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    return false;
}

bool FelixReorderer::do_avx_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(frames_start);
    UNUSED(frames_stop);
    return false;
}
#endif

#ifdef __AVX512__REMOVE_ME_AFTER_GCC_PATCH
void FelixReorderer::reorder_avx512_handle_four_frames_two_segments(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    /// Set up the two registers
    __m512i noshift = _mm512_set_epi8(src[m_frame3 + b_seg_1 + b_adc1_ch2_p1], src[m_frame3 + b_seg_1 + b_adc1_ch2_p0], src[m_frame2 + b_seg_1 + b_adc1_ch2_p1], src[m_frame2 + b_seg_1 + b_adc1_ch2_p0],
                                      src[m_frame1 + b_seg_1 + b_adc1_ch2_p1], src[m_frame1 + b_seg_1 + b_adc1_ch2_p0], src[m_frame0 + b_seg_1 + b_adc1_ch2_p1], src[m_frame0 + b_seg_1 + b_adc1_ch2_p0],

                                      src[m_frame3 + b_seg_1 + b_adc1_ch0_p1], src[m_frame3 + b_seg_1 + b_adc1_ch0_p0], src[m_frame2 + b_seg_1 + b_adc1_ch0_p1], src[m_frame2 + b_seg_1 + b_adc1_ch0_p0],
                                      src[m_frame1 + b_seg_1 + b_adc1_ch0_p1], src[m_frame1 + b_seg_1 + b_adc1_ch0_p0], src[m_frame0 + b_seg_1 + b_adc1_ch0_p1], src[m_frame0 + b_seg_1 + b_adc1_ch0_p0],

                                      src[m_frame3 + b_seg_0 + b_adc1_ch2_p1], src[m_frame3 + b_seg_0 + b_adc1_ch2_p0], src[m_frame2 + b_seg_0 + b_adc1_ch2_p1], src[m_frame2 + b_seg_0 + b_adc1_ch2_p0],
                                      src[m_frame1 + b_seg_0 + b_adc1_ch2_p1], src[m_frame1 + b_seg_0 + b_adc1_ch2_p0], src[m_frame0 + b_seg_0 + b_adc1_ch2_p1], src[m_frame0 + b_seg_0 + b_adc1_ch2_p0],

                                      src[m_frame3 + b_seg_0 + b_adc1_ch0_p1], src[m_frame3 + b_seg_0 + b_adc1_ch0_p0], src[m_frame2 + b_seg_0 + b_adc1_ch0_p1], src[m_frame2 + b_seg_0 + b_adc1_ch0_p0],
                                      src[m_frame1 + b_seg_0 + b_adc1_ch0_p1], src[m_frame1 + b_seg_0 + b_adc1_ch0_p0], src[m_frame0 + b_seg_0 + b_adc1_ch0_p1], src[m_frame0 + b_seg_0 + b_adc1_ch0_p0],

                                      src[m_frame3 + b_seg_1 + b_adc0_ch2_p1], src[m_frame3 + b_seg_1 + b_adc0_ch2_p0], src[m_frame2 + b_seg_1 + b_adc0_ch2_p1], src[m_frame2 + b_seg_1 + b_adc0_ch2_p0],
                                      src[m_frame1 + b_seg_1 + b_adc0_ch2_p1], src[m_frame1 + b_seg_1 + b_adc0_ch2_p0], src[m_frame0 + b_seg_1 + b_adc0_ch2_p1], src[m_frame0 + b_seg_1 + b_adc0_ch2_p0],

                                      src[m_frame3 + b_seg_1 + b_adc0_ch0_p1], src[m_frame3 + b_seg_1 + b_adc0_ch0_p0], src[m_frame2 + b_seg_1 + b_adc0_ch0_p1], src[m_frame2 + b_seg_1 + b_adc0_ch0_p0],
                                      src[m_frame1 + b_seg_1 + b_adc0_ch0_p1], src[m_frame1 + b_seg_1 + b_adc0_ch0_p0], src[m_frame0 + b_seg_1 + b_adc0_ch0_p1], src[m_frame0 + b_seg_1 + b_adc0_ch0_p0],

                                      src[m_frame3 + b_seg_0 + b_adc0_ch2_p1], src[m_frame3 + b_seg_0 + b_adc0_ch2_p0], src[m_frame2 + b_seg_0 + b_adc0_ch2_p1], src[m_frame2 + b_seg_0 + b_adc0_ch2_p0],
                                      src[m_frame1 + b_seg_0 + b_adc0_ch2_p1], src[m_frame1 + b_seg_0 + b_adc0_ch2_p0], src[m_frame0 + b_seg_0 + b_adc0_ch2_p1], src[m_frame0 + b_seg_0 + b_adc0_ch2_p0],

                                      src[m_frame3 + b_seg_0 + b_adc0_ch0_p1], src[m_frame3 + b_seg_0 + b_adc0_ch0_p0], src[m_frame2 + b_seg_0 + b_adc0_ch0_p1], src[m_frame2 + b_seg_0 + b_adc0_ch0_p0],
                                      src[m_frame1 + b_seg_0 + b_adc0_ch0_p1], src[m_frame1 + b_seg_0 + b_adc0_ch0_p0], src[m_frame0 + b_seg_0 + b_adc0_ch0_p1], src[m_frame0 + b_seg_0 + b_adc0_ch0_p0]);

    __m512i toshift = _mm512_set_epi8(src[m_frame3 + b_seg_1 + b_adc1_ch3_p1], src[m_frame3 + b_seg_1 + b_adc1_ch3_p0], src[m_frame2 + b_seg_1 + b_adc1_ch3_p1], src[m_frame2 + b_seg_1 + b_adc1_ch3_p0],
                                      src[m_frame1 + b_seg_1 + b_adc1_ch3_p1], src[m_frame1 + b_seg_1 + b_adc1_ch3_p0], src[m_frame0 + b_seg_1 + b_adc1_ch3_p1], src[m_frame0 + b_seg_1 + b_adc1_ch3_p0],

                                      src[m_frame3 + b_seg_1 + b_adc1_ch1_p1], src[m_frame3 + b_seg_1 + b_adc1_ch1_p0], src[m_frame2 + b_seg_1 + b_adc1_ch1_p1], src[m_frame2 + b_seg_1 + b_adc1_ch1_p0],
                                      src[m_frame1 + b_seg_1 + b_adc1_ch1_p1], src[m_frame1 + b_seg_1 + b_adc1_ch1_p0], src[m_frame0 + b_seg_1 + b_adc1_ch1_p1], src[m_frame0 + b_seg_1 + b_adc1_ch1_p0],

                                      src[m_frame3 + b_seg_0 + b_adc1_ch3_p1], src[m_frame3 + b_seg_0 + b_adc1_ch3_p0], src[m_frame2 + b_seg_0 + b_adc1_ch3_p1], src[m_frame2 + b_seg_0 + b_adc1_ch3_p0],
                                      src[m_frame1 + b_seg_0 + b_adc1_ch3_p1], src[m_frame1 + b_seg_0 + b_adc1_ch3_p0], src[m_frame0 + b_seg_0 + b_adc1_ch3_p1], src[m_frame0 + b_seg_0 + b_adc1_ch3_p0],

                                      src[m_frame3 + b_seg_0 + b_adc1_ch1_p1], src[m_frame3 + b_seg_0 + b_adc1_ch1_p0], src[m_frame2 + b_seg_0 + b_adc1_ch1_p1], src[m_frame2 + b_seg_0 + b_adc1_ch1_p0],
                                      src[m_frame1 + b_seg_0 + b_adc1_ch1_p1], src[m_frame1 + b_seg_0 + b_adc1_ch1_p0], src[m_frame0 + b_seg_0 + b_adc1_ch1_p1], src[m_frame0 + b_seg_0 + b_adc1_ch1_p0],

                                      src[m_frame3 + b_seg_1 + b_adc0_ch3_p1], src[m_frame3 + b_seg_1 + b_adc0_ch3_p0], src[m_frame2 + b_seg_1 + b_adc0_ch3_p1], src[m_frame2 + b_seg_1 + b_adc0_ch3_p0],
                                      src[m_frame1 + b_seg_1 + b_adc0_ch3_p1], src[m_frame1 + b_seg_1 + b_adc0_ch3_p0], src[m_frame0 + b_seg_1 + b_adc0_ch3_p1], src[m_frame0 + b_seg_1 + b_adc0_ch3_p0],

                                      src[m_frame3 + b_seg_1 + b_adc0_ch1_p1], src[m_frame3 + b_seg_1 + b_adc0_ch1_p0], src[m_frame2 + b_seg_1 + b_adc0_ch1_p1], src[m_frame2 + b_seg_1 + b_adc0_ch1_p0],
                                      src[m_frame1 + b_seg_1 + b_adc0_ch1_p1], src[m_frame1 + b_seg_1 + b_adc0_ch1_p0], src[m_frame0 + b_seg_1 + b_adc0_ch1_p1], src[m_frame0 + b_seg_1 + b_adc0_ch1_p0],

                                      src[m_frame3 + b_seg_0 + b_adc0_ch3_p1], src[m_frame3 + b_seg_0 + b_adc0_ch3_p0], src[m_frame2 + b_seg_0 + b_adc0_ch3_p1], src[m_frame2 + b_seg_0 + b_adc0_ch3_p0],
                                      src[m_frame1 + b_seg_0 + b_adc0_ch3_p1], src[m_frame1 + b_seg_0 + b_adc0_ch3_p0], src[m_frame0 + b_seg_0 + b_adc0_ch3_p1], src[m_frame0 + b_seg_0 + b_adc0_ch3_p0],

                                      src[m_frame3 + b_seg_0 + b_adc0_ch1_p1], src[m_frame3 + b_seg_0 + b_adc0_ch1_p0], src[m_frame2 + b_seg_0 + b_adc0_ch1_p1], src[m_frame2 + b_seg_0 + b_adc0_ch1_p0],
                                      src[m_frame1 + b_seg_0 + b_adc0_ch1_p1], src[m_frame1 + b_seg_0 + b_adc0_ch1_p0], src[m_frame0 + b_seg_0 + b_adc0_ch1_p1], src[m_frame0 + b_seg_0 + b_adc0_ch1_p0]);

    /// Shift 4 bits right, all bits align now
    __m512i afshift = _mm512_srai_epi32(toshift, 4);

    /// Mask upper four bits of each 16 bit part
    const __m512i mask = _mm512_set_epi16(0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0xfff,
                                          0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0xfff);
    __m512i v1 = _mm512_and_si512(noshift, mask);
    __m512i v2 = _mm512_and_si512(afshift, mask);

    /// Adressing vector
    __m512i addr1 = _mm512_set_epi64(14 * num_frames, 12 * num_frames, 10 * num_frames, 8 * num_frames, 6 * num_frames, 4 * num_frames, 2 * num_frames, 0 * num_frames);
    __m512i addr2 = _mm512_set_epi64(15 * num_frames, 13 * num_frames, 11 * num_frames, 9 * num_frames, 7 * num_frames, 5 * num_frames, 3 * num_frames, 1 * num_frames);

    _mm512_i64scatter_epi64(dst, addr1, v1, m_adc_size);
    _mm512_i64scatter_epi64(dst, addr2, v2, m_adc_size);
}

void FelixReorderer::reorder_avx512_handle_four_frames_one_block(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        reorder_avx512_handle_four_frames_two_segments(src + 2 * i * m_num_bytes_per_seg, dst + 2 * i * m_num_bytes_per_reord_seg * num_frames, num_frames);
    }
}

void FelixReorderer::reorder_avx512_handle_four_frames(const uint8_t *src, uint8_t *dst, unsigned frame_num, const unsigned &num_frames)
{
    /// Destinations
    uint8_t *wib_header_destination = dst + frame_num * m_wib_header_size;
    uint8_t *coldata_header_destination = dst + num_frames * m_wib_header_size + frame_num * m_coldata_header_size * m_num_blocks_per_frame;
    uint8_t *data_destination = dst + num_frames * (m_wib_header_size + m_coldata_header_size * m_num_blocks_per_frame) + frame_num * m_adc_size;

    /// Sources
    const uint8_t *coldata_start = src + m_wib_header_size;
    const uint8_t *data_start = coldata_start + m_coldata_header_size;

    /// Copies
    for (unsigned j = 0; j < m_num_blocks_per_frame; ++j)
    {
        memcpy(wib_header_destination + j * m_wib_header_size, src + j * m_num_bytes_per_frame, m_wib_header_size);

        for (unsigned i = 0; i < m_num_blocks_per_frame; ++i)
        {
            memcpy(coldata_header_destination + (j * m_num_blocks_per_frame + i) * m_coldata_header_size, coldata_start + i * (m_coldata_header_size + m_num_bytes_per_block) + j * m_num_bytes_per_frame, m_coldata_header_size);
        }
    }

    for (unsigned i = 0; i < m_num_blocks_per_frame; ++i)
    {
        reorder_avx512_handle_four_frames_one_block(data_start + i * (m_coldata_header_size + m_num_bytes_per_block),
                                                    data_destination + i * m_num_ch_per_block * m_adc_size * num_frames, num_frames);
    }
}

bool FelixReorderer::do_avx512_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames) noexcept
{
    try
    {
        for (unsigned i = 0; i < num_frames; i += 4)
        {
            reorder_avx512_handle_four_frames(src + i * m_num_bytes_per_frame, dst, i, num_frames);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorderer::do_avx512_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames) noexcept
{
    try
    {
        for (unsigned i = 0; i < frames_stop - frames_start; i += 4)
        {
            reorder_avx512_handle_four_frames(src + i * m_num_bytes_per_frame, dst, frames_start + i, num_frames);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}
#else
bool FelixReorderer::do_avx512_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);

    return false;
}

bool FelixReorderer::do_avx512_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(frames_start);
    UNUSED(frames_stop);

    return false;
}
#endif