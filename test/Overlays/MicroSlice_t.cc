#include "lbne-artdaq/Overlays/MicroSliceWriter.hh"
#include <vector>
#include <stdint.h>
#include <memory>

#define BOOST_TEST_MODULE(MicroSlice_t)
#include "boost/test/auto_unit_test.hpp"

BOOST_AUTO_TEST_SUITE(MicroSlice_test)

BOOST_AUTO_TEST_CASE(BaselineTest)
{
  const uint16_t CHANNEL_NUMBER = 123;
  const uint32_t MS_BUFFER_SIZE = 4096;
  const uint32_t NS_BUFFER_SIZE = 1024;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  const uint16_t SAMPLE3 = 0xbeef;
  const uint16_t SAMPLE4 = 0xfe87;
  const uint16_t SAMPLE5 = 0x5a5a;
  std::vector<uint8_t> work_buffer(MS_BUFFER_SIZE);
  std::unique_ptr<lbne::NanoSlice> nslice_ptr;
  std::shared_ptr<lbne::NanoSliceWriter> ns_writer_ptr;
  uint16_t value;

  // *** Use a MicroSliceWriter to build up a MicroSlice, checking
  // *** that everything looks good as we go.

  lbne::MicroSliceWriter ms_writer(&work_buffer[0], MS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.size(), sizeof(lbne::MicroSlice::Header));
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 0);
  nslice_ptr = ms_writer.nanoSlice(0);
  BOOST_REQUIRE(nslice_ptr.get() == 0);
  nslice_ptr = ms_writer.nanoSlice(999);
  BOOST_REQUIRE(nslice_ptr.get() == 0);

  ns_writer_ptr = ms_writer.reserveNanoSlice(NS_BUFFER_SIZE);
  BOOST_REQUIRE(ns_writer_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(ms_writer.size(),
                      sizeof(lbne::MicroSlice::Header) + NS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 1);
  if (ns_writer_ptr.get() != 0) {
    ns_writer_ptr->setChannelNumber(CHANNEL_NUMBER);
    ns_writer_ptr->addSample(SAMPLE1);
    ns_writer_ptr->addSample(SAMPLE2);
    ns_writer_ptr->addSample(SAMPLE3);
    ns_writer_ptr->addSample(SAMPLE4);
    ns_writer_ptr->addSample(SAMPLE5);

    uint32_t size_diff = ms_writer.finalize();
    BOOST_REQUIRE_EQUAL(size_diff, MS_BUFFER_SIZE - sizeof(lbne::MicroSlice::Header) -
                        sizeof(lbne::NanoSlice::Header) - 5*sizeof(uint16_t));

    nslice_ptr = ms_writer.nanoSlice(1);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(0);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
    if (nslice_ptr.get() != 0) {
      BOOST_REQUIRE_EQUAL(nslice_ptr->size(),
                          sizeof(lbne::MicroSlice::Header) + 5*sizeof(uint16_t));
      BOOST_REQUIRE_EQUAL(nslice_ptr->sampleCount(), 5);
      BOOST_REQUIRE(nslice_ptr->sampleValue(0, value));
      BOOST_REQUIRE_EQUAL(value, SAMPLE1);
      BOOST_REQUIRE(nslice_ptr->sampleValue(1, value));
      BOOST_REQUIRE_EQUAL(value, SAMPLE2);
      BOOST_REQUIRE(nslice_ptr->sampleValue(2, value));
      BOOST_REQUIRE_EQUAL(value, SAMPLE3);
      BOOST_REQUIRE(nslice_ptr->sampleValue(3, value));
      BOOST_REQUIRE_EQUAL(value, SAMPLE4);
      BOOST_REQUIRE(nslice_ptr->sampleValue(4, value));
      BOOST_REQUIRE_EQUAL(value, SAMPLE5);
    }
  }
}

BOOST_AUTO_TEST_CASE(MultipleNanoSliceTest)
{
  const uint16_t CHANNEL_NUMBER = 123;
  const uint32_t MS_BUFFER_SIZE = 4096;
  const uint32_t NS_BUFFER_SIZE = 1024;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  const uint16_t SAMPLE3 = 0xbeef;
  const uint16_t SAMPLE4 = 0xfe87;
  const uint16_t SAMPLE5 = 0x5a5a;
  std::vector<uint8_t> work_buffer(MS_BUFFER_SIZE);
  std::unique_ptr<lbne::NanoSlice> nslice_ptr;
  std::shared_ptr<lbne::NanoSliceWriter> ns_writer_ptr;
  uint16_t value;

  lbne::MicroSliceWriter ms_writer(&work_buffer[0], MS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.size(), sizeof(lbne::MicroSlice::Header));
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 0);

  ns_writer_ptr = ms_writer.reserveNanoSlice(NS_BUFFER_SIZE);
  BOOST_REQUIRE(ns_writer_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(ms_writer.size(),
                      sizeof(lbne::MicroSlice::Header) + NS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 1);
  if (ns_writer_ptr.get() != 0) {
    ns_writer_ptr->setChannelNumber(CHANNEL_NUMBER);
    ns_writer_ptr->addSample(SAMPLE1);
    ns_writer_ptr->addSample(SAMPLE2);
    ns_writer_ptr->addSample(SAMPLE3);
    ns_writer_ptr->addSample(SAMPLE4);
    ns_writer_ptr->addSample(SAMPLE5);

    nslice_ptr = ms_writer.nanoSlice(1);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(0);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
  }

  // *** Add a second NanoSlice

  ns_writer_ptr = ms_writer.reserveNanoSlice(NS_BUFFER_SIZE);
  BOOST_REQUIRE(ns_writer_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(ms_writer.size(), sizeof(lbne::MicroSlice::Header) +
                      sizeof(lbne::NanoSlice::Header) + 5*sizeof(uint16_t) +
                      NS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 2);
  if (ns_writer_ptr.get() != 0) {
    ns_writer_ptr->setChannelNumber(CHANNEL_NUMBER+1);
    ns_writer_ptr->addSample(SAMPLE5);
    ns_writer_ptr->addSample(SAMPLE3);
    ns_writer_ptr->addSample(SAMPLE1);

    nslice_ptr = ms_writer.nanoSlice(2);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(99);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(0);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
    nslice_ptr = ms_writer.nanoSlice(1);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
  }

  // *** Add a third NanoSlice

  ns_writer_ptr = ms_writer.reserveNanoSlice(NS_BUFFER_SIZE);
  BOOST_REQUIRE(ns_writer_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(ms_writer.size(), sizeof(lbne::MicroSlice::Header) +
                      2*sizeof(lbne::NanoSlice::Header) + 8*sizeof(uint16_t) +
                      NS_BUFFER_SIZE);
  BOOST_REQUIRE_EQUAL(ms_writer.nanoSliceCount(), 3);
  if (ns_writer_ptr.get() != 0) {
    ns_writer_ptr->setChannelNumber(CHANNEL_NUMBER+2);
    ns_writer_ptr->addSample(SAMPLE4);
    ns_writer_ptr->addSample(SAMPLE5);

    nslice_ptr = ms_writer.nanoSlice(3);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(99);
    BOOST_REQUIRE(nslice_ptr.get() == 0);
    nslice_ptr = ms_writer.nanoSlice(0);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
    nslice_ptr = ms_writer.nanoSlice(1);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
    nslice_ptr = ms_writer.nanoSlice(2);
    BOOST_REQUIRE(nslice_ptr.get() != 0);
  }

  // *** Finish off the creation of the MicroSlice and verify that we can't
  // *** add any more NanoSlices after it is finalized

  int32_t size_diff = ms_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, MS_BUFFER_SIZE - sizeof(lbne::MicroSlice::Header) -
                      3*sizeof(lbne::NanoSlice::Header) - 10*sizeof(uint16_t));
  ns_writer_ptr = ms_writer.reserveNanoSlice(NS_BUFFER_SIZE);
  BOOST_REQUIRE(ns_writer_ptr.get() == 0);

  // *** Now we construct an instance of a read-only MicroSlice from
  // *** the work buffer and verify that everything still looks good

  lbne::MicroSlice mslice(&work_buffer[0]);
  BOOST_REQUIRE_EQUAL(mslice.size(), sizeof(lbne::MicroSlice::Header) +
                      3*sizeof(lbne::NanoSlice::Header) + 10*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(mslice.nanoSliceCount(), 3);

  nslice_ptr = ms_writer.nanoSlice(0);
  BOOST_REQUIRE(nslice_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(nslice_ptr->size(), sizeof(lbne::NanoSlice::Header) +
                      5*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(nslice_ptr->channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(nslice_ptr->sampleCount(), 5);
  BOOST_REQUIRE(nslice_ptr->sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(nslice_ptr->sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(nslice_ptr->sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(nslice_ptr->sampleValue(3, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(nslice_ptr->sampleValue(4, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE5);
  BOOST_REQUIRE(! nslice_ptr->sampleValue(5, value));
  BOOST_REQUIRE(! nslice_ptr->sampleValue(567, value));

  nslice_ptr = ms_writer.nanoSlice(1);
  BOOST_REQUIRE(nslice_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(nslice_ptr->size(), sizeof(lbne::NanoSlice::Header) +
                      3*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(nslice_ptr->channelNumber(), CHANNEL_NUMBER+1);
  BOOST_REQUIRE_EQUAL(nslice_ptr->sampleCount(), 3);
  BOOST_REQUIRE(nslice_ptr->sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE5);
  BOOST_REQUIRE(nslice_ptr->sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(nslice_ptr->sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(! nslice_ptr->sampleValue(3, value));

  nslice_ptr = ms_writer.nanoSlice(2);
  BOOST_REQUIRE(nslice_ptr.get() != 0);
  BOOST_REQUIRE_EQUAL(nslice_ptr->size(), sizeof(lbne::NanoSlice::Header) +
                      2*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(nslice_ptr->channelNumber(), CHANNEL_NUMBER+2);
  BOOST_REQUIRE_EQUAL(nslice_ptr->sampleCount(), 2);
  BOOST_REQUIRE(nslice_ptr->sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(nslice_ptr->sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE5);
  BOOST_REQUIRE(! nslice_ptr->sampleValue(2, value));
}

#if 0
BOOST_AUTO_TEST_CASE(TinyBufferTest)
{

  // *** Test a buffer that is too small for even the header

}

BOOST_AUTO_TEST_CASE(SmallBufferTest)
{

  // *** Test a buffer that is too small for two values

}

BOOST_AUTO_TEST_CASE(CopyTest)
{
}

BOOST_AUTO_TEST_CASE(BufferReuseTest)
{

  // *** If we reuse a buffer, then all history of the earlier MicroSlice
  // *** should be gone


  // *** And, if we try to use the earlier Writer after the buffer has
  // *** been reused, that shouldn't work either

}
#endif

BOOST_AUTO_TEST_SUITE_END()
