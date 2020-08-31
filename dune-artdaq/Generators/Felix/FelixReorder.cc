/// Thijs Miedema, last edit 2018-08-30

#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "FelixReorder.hh"

#define UNUSED(x) (void)(x)


void FelixReorder::copy_headers(uint8_t *dst, const uint8_t *src) {
    /// WIB
    memcpy(dst, src, m_wib_header_size);

    /// ColData
    for(unsigned i=0; i < m_num_blocks_per_frame; ++i) {
        memcpy(dst + m_wib_header_size + i * m_coldata_header_size,
               src + m_wib_header_size + i * (m_num_bytes_per_block + m_coldata_header_size),
               m_coldata_header_size);
    }
}

void FelixReorder::handle_headers(uint8_t* dst, const uint8_t* src, const unsigned frame_index, const unsigned &num_frames, unsigned* num_faulty) {
    size_t bitfield_size = (num_frames + 7) / 8;
    
    if (frame_index == 0) {
        for(unsigned i = 0; i < bitfield_size; i++)
            dst[i] = 0;
        copy_headers(dst + bitfield_size, src);
        return;
    }
    
    const dune::WIBHeader *wib_frame_0 = reinterpret_cast<dune::WIBHeader const *>(dst + bitfield_size);
    const dune::WIBHeader *wib_frame_n = reinterpret_cast<dune::WIBHeader const *>(src);
    
    bool check_failed = false;

    // WIB header checks.
    check_failed |= wib_frame_0->sof ^ wib_frame_n->sof;
    check_failed |= wib_frame_0->version ^ wib_frame_n->version;
    check_failed |= wib_frame_0->fiber_no ^ wib_frame_n->fiber_no;
    check_failed |= wib_frame_0->crate_no ^ wib_frame_n->crate_no;
    check_failed |= wib_frame_0->slot_no ^ wib_frame_n->slot_no;
    check_failed |= wib_frame_0->mm ^ wib_frame_n->mm;
    check_failed |= wib_frame_0->oos ^ wib_frame_n->oos;
    check_failed |= wib_frame_0->wib_errors ^ wib_frame_n->wib_errors;
    check_failed |= wib_frame_0->z ^ wib_frame_n->z;
    check_failed |= (uint64_t)(wib_frame_0->timestamp() + 25 * frame_index) ^ wib_frame_n->timestamp();
    

    // COLDATA header checks.
    for (unsigned j = 0; j < 4; ++j) {
        const dune::ColdataHeader *col_frame_0 = reinterpret_cast<dune::ColdataHeader const *>
            (dst + bitfield_size + m_wib_header_size + j * m_coldata_header_size);
        const dune::ColdataHeader *col_frame_n = reinterpret_cast<dune::ColdataHeader const *>
            (src + m_wib_header_size + j * (m_coldata_header_size + m_num_bytes_per_block));
        
        check_failed |= col_frame_0->s1_error ^ col_frame_n->s1_error;
        check_failed |= col_frame_0->s2_error ^ col_frame_n->s2_error;
        check_failed |= col_frame_0->checksum_a() ^ col_frame_n->checksum_a();
        check_failed |= col_frame_0->checksum_b() ^ col_frame_n->checksum_b();
        check_failed |= col_frame_0->error_register ^ col_frame_n->error_register;

        for (unsigned h = 0; h < 8; ++h) {
            check_failed |= col_frame_0->hdr(h) ^ col_frame_n->hdr(h);
        }
        check_failed |=
            (uint16_t)(col_frame_0->coldata_convert_count + 1 * frame_index) ^
            col_frame_n->coldata_convert_count;
    }

    if (check_failed) {
        copy_headers(dst + bitfield_size + (*num_faulty + 1) * 
            (m_wib_header_size + m_num_blocks_per_frame * m_coldata_header_size), src);

        (*num_faulty)++;
        dst[frame_index/8] |= 1 << (frame_index % 8);
    }
}

void FelixReorder::baseline_handle_frames(uint8_t *dst, const uint8_t *psrc, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty)
{
    // Store all ADC values in uint16_t.
    const dune::FelixFrame *src = reinterpret_cast<dune::FelixFrame const *>(psrc);
    uint8_t *end = dst + num_frames * m_num_bytes_per_data;

    for (unsigned fr = frames_start; fr < frames_stop; ++fr)
    {
        handle_headers(end, psrc + m_num_bytes_per_frame * (fr - frames_start), fr, num_frames, num_faulty);

        for (unsigned ch = 0; ch < m_num_ch_per_frame; ++ch)
        {
            dune::adc_t curr_val = (src + fr - frames_start)->channel(ch);
            memcpy(dst + (ch * num_frames + fr) * m_adc_size, &curr_val, m_adc_size);
        }
    }
}

bool FelixReorder::do_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        baseline_handle_frames(dst, src, 0, num_frames, num_frames, num_faulty);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorder::do_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        baseline_handle_frames(dst, src, frames_start, frames_stop, num_frames, num_faulty);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

#ifdef __AVX2__
void FelixReorder::reorder_avx_handle_four_segments(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
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

void FelixReorder::reorder_avx_handle_block(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    reorder_avx_handle_four_segments(src, dst, num_frames);
    reorder_avx_handle_four_segments(src + 4 * m_num_bytes_per_seg, dst + 4 * m_num_bytes_per_reord_seg * num_frames, num_frames);
}

void FelixReorder::reorder_avx_handle_frame(const uint8_t *src, uint8_t *dst, unsigned frame_num, const unsigned &num_frames, unsigned *num_faulty)
{
    /// Destinations
    uint8_t *data_destination = dst + frame_num * m_adc_size;
    uint8_t *header_destination = dst + num_frames * m_num_bytes_per_data;

    /// Sources
    const uint8_t *data_start = src + m_wib_header_size + m_coldata_header_size;

    handle_headers(header_destination, src, frame_num, num_frames, num_faulty);

    for (unsigned i = 0; i < m_num_blocks_per_frame; ++i)
    {
        reorder_avx_handle_block(data_start + i * (m_coldata_header_size + m_num_bytes_per_block),
                                 data_destination + i * m_num_ch_per_block * m_adc_size * num_frames, num_frames);
    }
}

bool FelixReorder::do_avx_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        for (unsigned i = 0; i < num_frames; i++)
        {
            reorder_avx_handle_frame(src + i * m_num_bytes_per_frame, dst, i, num_frames, num_faulty);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorder::do_avx_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        for (unsigned i = 0; i < frames_stop - frames_start; i++)
        {
            reorder_avx_handle_frame(src + i * m_num_bytes_per_frame, dst, frames_start + i, num_frames, num_faulty);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

#else
bool FelixReorder::do_avx_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(num_faulty);
    return false;
}

bool FelixReorder::do_avx_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(frames_start);
    UNUSED(frames_stop);
    UNUSED(num_faulty);

    return false;
}
#endif

#ifdef __AVX512__REMOVE_ME_AFTER_GCC_PATCH
void FelixReorder::reorder_avx512_handle_four_frames_two_segments(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
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

void FelixReorder::reorder_avx512_handle_four_frames_one_block(const uint8_t *src, uint8_t *dst, const unsigned &num_frames)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        reorder_avx512_handle_four_frames_two_segments(src + 2 * i * m_num_bytes_per_seg, dst + 2 * i * m_num_bytes_per_reord_seg * num_frames, num_frames);
    }
}

void FelixReorder::reorder_avx512_handle_four_frames(const uint8_t *src, uint8_t *dst, unsigned frame_num, const unsigned &num_frames, unsigned *num_faulty)
{
    /// Destinations
    uint8_t *data_destination = dst + frame_num * m_adc_size;

    /// Sources
    const uint8_t *data_start = src + m_wib_header_size + m_coldata_header_size;
    uint8_t *end = dst + num_frames * m_num_bytes_per_data;

    /// Copies
    for (unsigned j = 0; j < 4; ++j)
    {
        handle_headers(end, src + j * m_num_bytes_per_frame, frame_num + j, num_frames, num_faulty);
    }

    for (unsigned i = 0; i < m_num_blocks_per_frame; ++i)
    {
        reorder_avx512_handle_four_frames_one_block(data_start + i * (m_coldata_header_size + m_num_bytes_per_block),
                                                    data_destination + i * m_num_ch_per_block * m_adc_size * num_frames, num_frames);
    }
}

bool FelixReorder::do_avx512_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        for (unsigned i = 0; i < num_frames; i += 4)
        {
            reorder_avx512_handle_four_frames(src + i * m_num_bytes_per_frame, dst, i, num_frames, num_faulty);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FelixReorder::do_avx512_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    try
    {
        for (unsigned i = 0; i < frames_stop - frames_start; i += 4)
        {
            reorder_avx512_handle_four_frames(src + i * m_num_bytes_per_frame, dst, frames_start + i, num_frames, num_faulty);
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}
#else
bool FelixReorder::do_avx512_reorder(uint8_t *dst, const uint8_t *src, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(num_faulty);

    return false;
}

bool FelixReorder::do_avx512_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned &num_frames, unsigned *num_faulty) noexcept
{
    UNUSED(dst);
    UNUSED(src);
    UNUSED(num_frames);
    UNUSED(frames_start);
    UNUSED(frames_stop);
    UNUSED(num_faulty);

    return false;
}
#endif

