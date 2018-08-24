#ifndef FELIX_REORDERER_FACILITY_HH_
#define FELIX_REORDERER_FACILITY_HH_

/*
 * FelixReordererFacility
 * Author: Thijs Miedema
 * Description: Simple wrapper around FelixReorderer to make it plug and play
 * Date: July 2018
*/

#include <vector>
#include <chrono>
#include <atomic>
#include "FelixReorderer.hh"
#include "SlidingAverage.hh"

enum ReordererType {
    TypeMILO,
    TypeAVX2,
    TypeAVX512
};

class FelixReordererFacility {
  public:
    FelixReordererFacility(ReordererType reordertype): mytype(reordertype), timing(100), time_average(0) {}
    
    bool do_reorder(uint8_t *dst, uint8_t *src, const unsigned num_frames=10000) {
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
        bool success = inner_do_reorder(dst, src, num_frames);
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);

        /// Update rate
        timing.add_value(time_span.count());
        time_average.store(timing.avg(), std::memory_order_relaxed);

        return success;
    }

    bool do_reorder_no_timing(uint8_t *dst, uint8_t *src, const unsigned num_frames) {
        return inner_do_reorder(dst, src, num_frames);
    }

  private:
    bool inner_do_reorder(uint8_t *dst, uint8_t *src, const unsigned num_frames) {
        switch(mytype) {
            case ReordererType::TypeAVX512:
                if (FelixReorderer::avx512_available)
                    return FelixReorderer::do_avx512_reorder(dst, src, num_frames);
            case ReordererType::TypeAVX2:
                if (FelixReorderer::avx_available)
                    return FelixReorderer::do_avx_reorder(dst, src, num_frames);
            case ReordererType::TypeMILO:
                return FelixReorderer::do_reorder(dst, src, num_frames);  
        }
        return false;
    }

    ReordererType mytype; 
    SlidingAverage<double> timing;
  public:
    /// Expose for statistics service ///
    std::atomic<double> time_average;
};

#endif /* FELIX_REORDERER_FACILITY_HH_ */
