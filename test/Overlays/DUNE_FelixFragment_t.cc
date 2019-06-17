#include <stdint.h>
#include <bitset>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/FelixReorder.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

#define BOOST_TEST_MODULE(MilliSlice_t)
#ifdef HAVE_CANVAS
#include "cetlib/quiet_unit_test.hpp"
#else
#include "boost/test/auto_unit_test.hpp"
#endif

BOOST_AUTO_TEST_SUITE(FelixFragment_test)

BOOST_AUTO_TEST_CASE(BaselineTest) {
  std::cout << "### WOOF -> Testing fragment creation from file...\n";

  std::ifstream in(
      "/nashome/m/milov/Documents/kevlar/run/Run_1-SubRun_6120-Event_3059515-Plane_0-Frame_1.dat",
      std::ios::binary);
  if(!in.is_open()) {
    std::cout << "Could not open file.\n";
    return;
  }
  std::string contents((std::istreambuf_iterator<char>(in)),
                       (std::istreambuf_iterator<char>()));

  dune::FelixFragmentBase::Metadata meta;
  std::unique_ptr<artdaq::Fragment> frag_ptr(artdaq::Fragment::FragmentBytes(
      contents.size(), 1, 1, dune::toFragmentType("FELIX"), meta));
  frag_ptr->resizeBytes(contents.size());
  memcpy(frag_ptr->dataBeginBytes(), contents.c_str(), contents.size());
  in.close();

  dune::FelixFragment flxfrg(*frag_ptr);

  // std::cout << "\n\nPrinting the first frame's contents.\n";
  // flxfrg.print(0);

  std::cout << "### WOOF -> Test for the presence of 9600 frames...\n";
  const size_t frames = 9600;

  std::cout << "  -> Total words: " << flxfrg.total_words() << '\n';
  std::cout << "  -> Total frames: " << flxfrg.total_frames() << '\n';
  std::cout << "  -> Total adc values: " << flxfrg.total_adc_values() << '\n';

  BOOST_REQUIRE_EQUAL(flxfrg.total_words(), frames*120);
  BOOST_REQUIRE_EQUAL(flxfrg.total_frames(), frames);
  BOOST_REQUIRE_EQUAL(flxfrg.total_adc_values(), frames*256);
  std::cout << "\n\n";

  std::cout << "### WOOF -> WIB frame test...\n";
  BOOST_REQUIRE_EQUAL(sizeof(dune::FelixFrame), 480);
  std::cout << " -> SOF: " << unsigned(flxfrg.sof(0)) << "\n";
  std::cout << " -> Version: " << unsigned(flxfrg.version(0)) << "\n";
  std::cout << " -> FiberNo: " << unsigned(flxfrg.fiber_no(0)) << "\n";
  std::cout << " -> SlotNo: " << unsigned(flxfrg.slot_no(0)) << "\n";
  std::cout << " -> CrateNo: " << unsigned(flxfrg.crate_no(0)) << "\n";
  std::cout << " -> Timestamp: " << std::hex << flxfrg.timestamp(0) << std::dec;
  std::cout << "\n\n";

  // std::cout << "### WOOF -> Dumping frames...\n";
  // flxfrg.print_frames();
  // std::cout << "\n\n";

  // Test whether FelixFragment and FelixFragmentReordered correspond.
  std::cout << "### MEOW -> Reordered FELIX Fragment test.\n";

  std::unique_ptr<artdaq::Fragment> frag_ptr2(artdaq::Fragment::FragmentBytes(
      contents.size(), 1, 1, dune::toFragmentType("FELIX"), meta));
  frag_ptr2->resizeBytes(contents.size());
  memcpy(frag_ptr2->dataBeginBytes(), contents.c_str(), contents.size());
  in.close();

  artdaq::Fragment reordfrg(dune::FelixReorder(frag_ptr2->dataBeginBytes(), frames));
  dune::FelixFragment reordflxfrg(reordfrg, 1);

  std::cout << "  -> Total words: " << reordflxfrg.total_words() << '\n';
  std::cout << "  -> Total frames: " << reordflxfrg.total_frames() << '\n';
  std::cout << "  -> Total adc values: " << reordflxfrg.total_adc_values()
            << '\n';

  std::cout << "### MEOW -> Comparing " << frames << " frames.\n";
  auto comp_begin = std::chrono::high_resolution_clock::now();
  for (unsigned i = 0; i < frames; ++i) {
    BOOST_REQUIRE_EQUAL(flxfrg.sof(i), reordflxfrg.sof(i));
    BOOST_REQUIRE_EQUAL(flxfrg.version(i), reordflxfrg.version(i));
    BOOST_REQUIRE_EQUAL(flxfrg.fiber_no(i), reordflxfrg.fiber_no(i));
    BOOST_REQUIRE_EQUAL(flxfrg.slot_no(i), reordflxfrg.slot_no(i));
    BOOST_REQUIRE_EQUAL(flxfrg.crate_no(i), reordflxfrg.crate_no(i));
    BOOST_REQUIRE_EQUAL(flxfrg.timestamp(i), reordflxfrg.timestamp(i));
    BOOST_REQUIRE_EQUAL(flxfrg.CRC(i), reordflxfrg.CRC(i));
    for (unsigned j = 0; j < 4; ++j) {
      BOOST_REQUIRE_EQUAL(flxfrg.s1_error(i, j), reordflxfrg.s1_error(i, j));
      BOOST_REQUIRE_EQUAL(flxfrg.s2_error(i, j), reordflxfrg.s2_error(i, j));
      BOOST_REQUIRE_EQUAL(flxfrg.checksum_a(i, j),
                          reordflxfrg.checksum_a(i, j));
      BOOST_REQUIRE_EQUAL(flxfrg.checksum_b(i, j),
                          reordflxfrg.checksum_b(i, j));
      BOOST_REQUIRE_EQUAL(flxfrg.coldata_convert_count(i, j),
                          reordflxfrg.coldata_convert_count(i, j));
      BOOST_REQUIRE_EQUAL(flxfrg.error_register(i, j),
                          reordflxfrg.error_register(i, j));
      for (unsigned h = 0; h < 8; ++h) {
        BOOST_REQUIRE_EQUAL(flxfrg.hdr(i, j, h), reordflxfrg.hdr(i, j, h));
      }
    }
    for (unsigned ch = 0; ch < 256; ++ch) {
      BOOST_REQUIRE_EQUAL(flxfrg.get_ADC(i, ch), reordflxfrg.get_ADC(i, ch));
    }
  }
  auto comp_end = std::chrono::high_resolution_clock::now();
  std::cout << "### MEOW -> Tests successful.\n";
  std::cout << "Took " << std::chrono::duration_cast<std::chrono::milliseconds>(comp_end-comp_begin).count() << " ms.\n";

  // // Write reordered fragment to file for compression testing.
  // std::ofstream ofile("/afs/cern.ch/work/m/mivermeu/private/dune-raw-data/frames/reordered.dat");
  // ofile.write(reinterpret_cast<char const*>(reordflxfrg.head),
  //             reordflxfrg.total_words() * 4);
  // ofile.close();

  std::cout << "### WOOF WOOF -> Done...\n";
}

#if 0
BOOST_AUTO_TEST_CASE(TinyBufferTest)
{

  // *** Test a buffer that is too small for even the header

}

BOOST_AUTO_TEST_CASE(SmallBufferTest)
{

  // *** Test a buffer that is too small for two microslices

}

BOOST_AUTO_TEST_CASE(CopyTest)
{
}

BOOST_AUTO_TEST_CASE(BufferReuseTest)
{
}
#endif

BOOST_AUTO_TEST_SUITE_END()

#pragma GCC diagnostic pop
