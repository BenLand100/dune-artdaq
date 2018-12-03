#ifndef PROCESS_AVX2_H
#define PROCESS_AVX2_H

#include "ProcessingInfo.h"
#include "frame_expand.h"
#include "immintrin.h"
#include "constants.h"

void 
frugal_accum_update_avx2(__m256i& median, const __m256i s, __m256i& accum, const int16_t acclimit,
                              const __m256i mask);

ProcessingInfo
process_window_avx2(ProcessingInfo info_in);

#endif
