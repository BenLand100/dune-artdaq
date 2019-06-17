#ifndef REORDER_FACILITY_HH_
#define REORDER_FACILITY_HH_

/*
 * ReorderFacility
 * Author: Thijs Miedema
 * Description: Simple wrapper around FelixReorder to make it plug and play
 * Date: July 2018
 */

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "FelixReorder.hh"
#include "artdaq-core/Data/Fragment.hh"

namespace dune {

class ReorderFacility {
 public:
  ReorderFacility(bool force_no_avx = false) : m_force_no_avx(force_no_avx) {}

  bool do_reorder(uint8_t *dst, const uint8_t *src, const unsigned num_frames) {
    if (m_force_no_avx) {
      return FelixReorder::do_reorder(dst, src, num_frames,
                                      &m_num_faulty_frames);
    }
    if (FelixReorder::avx512_available) {
      return FelixReorder::do_avx512_reorder(dst, src, num_frames,
                                             &m_num_faulty_frames);
    }
    if (FelixReorder::avx_available) {
      return FelixReorder::do_avx_reorder(dst, src, num_frames,
                                          &m_num_faulty_frames);
    }
    return FelixReorder::do_reorder(dst, src, num_frames, &m_num_faulty_frames);
  }

  void do_reorder_start(unsigned num_frames) {
    m_num_faulty_frames = 0;
    m_num_frames = num_frames;
  }

  unsigned reorder_final_size() {
    return FelixReorder::calculate_reordered_size(m_num_frames,
                                                  m_num_faulty_frames);
  }

  bool do_reorder_part(uint8_t *dst, const uint8_t *src, const unsigned frames_start,
                       const unsigned frames_stop, const unsigned num_frames) {
    if (m_force_no_avx) {
      return FelixReorder::do_reorder_part(dst, src, frames_start, frames_stop,
                                           num_frames, &m_num_faulty_frames);
    }
    if (FelixReorder::avx512_available) {
      return FelixReorder::do_avx512_reorder_part(dst, src, frames_start,
                                                  frames_stop, num_frames,
                                                  &m_num_faulty_frames);
    }
    if (FelixReorder::avx_available) {
      return FelixReorder::do_avx_reorder_part(dst, src, frames_start,
                                               frames_stop, num_frames,
                                               &m_num_faulty_frames);
    }
    return FelixReorder::do_reorder_part(dst, src, frames_start, frames_stop,
                                         num_frames, &m_num_faulty_frames);
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

artdaq::Fragment FelixReorder(const uint8_t *src,
                              const uint16_t &num_frames = 6000) {
  artdaq::Fragment result;
  dune::FelixFragmentBase::Metadata meta = {0xabc, 1, 1, 0, num_frames, 0, num_frames};
  result.setMetadata(meta);
  result.resizeBytes(num_frames * (256*sizeof(adc_t) + sizeof(WIBHeader) + 4*sizeof(ColdataHeader))+ (num_frames+7)/8);
  uint8_t *dest = result.dataBeginBytes();

  ReorderFacility facility(false);
  facility.do_reorder_start(num_frames);
  facility.do_reorder(dest, src, num_frames);
  std::cout << "INFO: " << facility.get_info() << '\n';
  unsigned size = facility.reorder_final_size();
  result.resizeBytes(size);

  return result;
}

} // namespace dune

#endif /* REORDER_FACILITY_HH_ */
