#ifndef REORDERER_FACILITY_HH_
#define REORDERER_FACILITY_HH_

/*
 * ReordererFacility
 * Author: Thijs Miedema
 * Description: Simple wrapper around FelixReorderer to make it plug and play
 * Date: July 2018
*/

#include <vector>
#include <chrono>
#include <atomic>
#include <string>

#include "FelixReorderer.hh"

class ReordererFacility {
  public:
    ReordererFacility(bool _force_no_avx): force_no_avx(_force_no_avx) {}

    bool do_reorder(uint8_t *dst, uint8_t *src, const unsigned num_frames) {
        if (force_no_avx) {
            return FelixReorderer::do_reorder(dst, src, num_frames);
        }
        if (FelixReorderer::avx512_available) {
            return FelixReorderer::do_avx512_reorder(dst, src, num_frames);
        }
        if (FelixReorderer::avx_available){
            return FelixReorderer::do_avx_reorder(dst, src, num_frames);
        }
        return FelixReorderer::do_reorder(dst, src, num_frames);  
    }

    bool do_reorder_part(uint8_t *dst, uint8_t *src, const unsigned frames_start, const unsigned frames_stop, const unsigned num_frames) {
        if (force_no_avx) {
            return FelixReorderer::do_reorder_part(dst, src, frames_start, frames_stop, num_frames);  
        }
        if (FelixReorderer::avx512_available) {
            return FelixReorderer::do_avx512_reorder_part(dst, src, frames_start, frames_stop, num_frames);
        }
        if (FelixReorderer::avx_available) {
            return FelixReorderer::do_avx_reorder_part(dst, src, frames_start, frames_stop, num_frames);
        }
        return FelixReorderer::do_reorder_part(dst, src, frames_start, frames_stop, num_frames);  
    }

    std::string get_info() {
        if (force_no_avx) {
            return "Forced by config to not use AVX.";
        }
        if (FelixReorderer::avx512_available) {
            return "Going to use AVX512.";
        }
        if (FelixReorderer::avx_available) {
            return "Going to use AVX2.";
        }
        return "Going to use baseline.";
    }

  private:
    bool force_no_avx; 
};

#endif /* REORDERER_FACILITY_HH_ */
