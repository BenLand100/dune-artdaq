#ifndef REORDER_FACILITY_HH_
#define REORDER_FACILITY_HH_

/*
 * ReorderFacility
 * Author: Thijs Miedema
 * Description: Simple wrapper around FelixReorder to make it plug and play
 * Date: July 2018
*/

#include <vector>
#include <chrono>
#include <atomic>
#include <string>

#include "FelixReorder.hh"

class ReorderFacility {
  public:
    ReorderFacility(bool force_no_avx=false): m_force_no_avx(force_no_avx) {}

    bool do_reorder(uint8_t *dst, uint8_t *src, const unsigned num_frames) {
        if (m_force_no_avx) {
            return FelixReorder::do_reorder(dst, src, num_frames, &m_num_faulty_frames);
        }
        if (FelixReorder::avx512_available) {
            return FelixReorder::do_avx512_reorder(dst, src, num_frames, &m_num_faulty_frames);
        }
        if (FelixReorder::avx_available){
            return FelixReorder::do_avx_reorder(dst, src, num_frames, &m_num_faulty_frames);
        }
        return FelixReorder::do_reorder(dst, src, num_frames, &m_num_faulty_frames);  
    }

    void do_reorder_start(unsigned num_frames) {
        m_num_faulty_frames = 0;
        m_num_frames = num_frames;
    }

    unsigned reorder_final_size() {
        return FelixReorder::calculate_reordered_size(m_num_frames, m_num_faulty_frames);
    }

    bool do_reorder_part(uint8_t *dst, uint8_t *src, const unsigned frames_start, const unsigned frames_stop) {
        if (m_force_no_avx) {
            return FelixReorder::do_reorder_part(dst, src, frames_start, frames_stop, m_num_frames, &m_num_faulty_frames);  
        }
        if (FelixReorder::avx512_available) {
            return FelixReorder::do_avx512_reorder_part(dst, src, frames_start, frames_stop, m_num_frames, &m_num_faulty_frames);
        }
        if (FelixReorder::avx_available) {
            return FelixReorder::do_avx_reorder_part(dst, src, frames_start, frames_stop, m_num_frames, &m_num_faulty_frames);
        }
        return FelixReorder::do_reorder_part(dst, src, frames_start, frames_stop, m_num_frames, &m_num_faulty_frames);  
    }

    std::string get_info() {
        if (m_force_no_avx) {
            return "Forced by config to not use AVX.";
        }
        if (FelixReorder::avx512_available) {
            return "Going to use AVX512.";
        }
        if (FelixReorder::avx_available) {
            return "Going to use AVX2.";
        }
        return "Going to use baseline.";
    }

    unsigned m_num_faulty_frames;
    unsigned m_num_frames;
  private:
    bool m_force_no_avx; 
};

#endif /* REORDER_FACILITY_HH_ */

