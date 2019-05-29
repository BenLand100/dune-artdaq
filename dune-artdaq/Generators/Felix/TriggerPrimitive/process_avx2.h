#ifndef PROCESS_AVX2_H
#define PROCESS_AVX2_H

#include "ProcessingInfo.h"
#include "frame_expand.h"
#include "immintrin.h"
#include "constants.h"

/* void 
frugal_accum_update_avx2(__m256i& __restrict__ median, const __m256i s, __m256i&  __restrict__ accum, const int16_t acclimit,
                         const __m256i mask) __attribute__((always_inline));
*/
void
process_window_avx2(ProcessingInfo& info);

#endif
