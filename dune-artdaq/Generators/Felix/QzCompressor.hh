#ifndef QZCOMPRESSOR_HH_
#define QZCOMPRESSOR_HH_

#include "dune-raw-data/Overlays/FragmentType.hh"
#include <cstddef> // qatzip.h needs it (size_t)
#include "qatzip.h"
#include <vector>
#include <mutex>
#include <fstream>

#define FLXQATDEBUG

/*
 * QzCompressor
 * Author: Fabrice Le Goff - flegoff@cern.ch
 * Added by: Roland.Sipos@cern.ch
 * Description: Compression facility, using Intel QAT.
 * Date: August 2018
 *
 * Comments: changed to accept fragments instead of raw pointers.
*/


class QzCompressor {
public:
  enum class QzAlgo { Deflate, Snappy, LZ4 };

  /* level: [1..9], default: 1
   * input_size_threshold: input data size below which software compression
   * is invoked rather than hardware compression
   */
  QzCompressor(QzAlgo algo = QzAlgo::Deflate,
               unsigned int level = QZ_COMP_LEVEL_DEFAULT,
               unsigned int hw_buffer_size_kb = QZ_HW_BUFF_SZ / 1024,
               unsigned int input_size_threshold = QZ_COMP_THRESHOLD_DEFAULT);

  ~QzCompressor();
  QzCompressor(QzCompressor const &) = delete;

  int init(unsigned max_expected_fragment_size, int engine);
  int shutdown();

  uint_fast32_t do_compress(artdaq::Fragment* fragPtr, uint_fast32_t fragSize);

private:
  QzSession qzsession_;
  QzSessionParams qzparams_;

  unsigned internal_buffer_size_;
  uint8_t* internal_buffer_; // align to 64B? 

  bool init_;
  std::mutex init_mutex_;
};

#endif /* QZCOMPRESSOR_HH_ */

