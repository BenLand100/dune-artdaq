#include "lbne-artdaq/Overlays/NanoSliceWriter.hh"
#include <vector>
#include <stdint.h>

#define BOOST_TEST_MODULE(NanoSlice_t)
#include "boost/test/auto_unit_test.hpp"

BOOST_AUTO_TEST_SUITE(NanoSlice_test)

BOOST_AUTO_TEST_CASE(BaselineTest)
{
  const uint16_t CHANNEL_NUMBER = 123;
  const uint32_t BUFFER_SIZE = 4096;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  const uint16_t SAMPLE3 = 0xbeef;
  const uint16_t SAMPLE4 = 0xfe87;
  const uint16_t SAMPLE5 = 0x5a5a;
  std::vector<uint8_t> work_buffer(BUFFER_SIZE);
  uint16_t value;

  // *** Use a NanoSliceWriter to build up a NanoSlice, checking
  // *** that everything looks good as we go.

  lbne::NanoSliceWriter ns_writer(&work_buffer[0], BUFFER_SIZE, 0);
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), 0);
  BOOST_REQUIRE(! ns_writer.sampleValue(0, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(999, value));

  ns_writer.setChannelNumber(CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);

  BOOST_REQUIRE(ns_writer.addSample(SAMPLE1));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 1);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(! ns_writer.sampleValue(1, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(999, value));

  BOOST_REQUIRE(ns_writer.addSample(SAMPLE2));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + 2*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 2);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(ns_writer.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(! ns_writer.sampleValue(2, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(101, value));

  BOOST_REQUIRE(ns_writer.addSample(SAMPLE3));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + 3*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 3);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(ns_writer.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(ns_writer.sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(! ns_writer.sampleValue(3, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(101, value));

  BOOST_REQUIRE(ns_writer.addSample(SAMPLE4));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + 4*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 4);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(ns_writer.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(ns_writer.sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(ns_writer.sampleValue(3, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(! ns_writer.sampleValue(4, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(101, value));

  // *** Finish off the creation of the NanoSlice and verify that we can't
  // *** add any more samples after it is finalized

  int32_t size_diff = ns_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, BUFFER_SIZE -
                      sizeof(lbne::NanoSlice::Header) - 4*sizeof(uint16_t));
  BOOST_REQUIRE(! ns_writer.addSample(SAMPLE5));

  // *** Now let's construct an instance of a read-only NanoSlice from
  // *** the work buffer and verify that everything still looks good

  lbne::NanoSlice nslice(&work_buffer[0]);
  BOOST_REQUIRE_EQUAL(nslice.size(),
                      sizeof(lbne::NanoSlice::Header) + 4*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(nslice.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(nslice.sampleCount(), 4);
  BOOST_REQUIRE(nslice.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(nslice.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(nslice.sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(nslice.sampleValue(3, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(! nslice.sampleValue(4, value));
  BOOST_REQUIRE(! nslice.sampleValue(567, value));
}

BOOST_AUTO_TEST_CASE(TinyBufferTest)
{
  const uint16_t CHANNEL_NUMBER = 456;
  const uint32_t BUFFER_SIZE = 4096;
  const uint32_t SMALL_SIZE1 = 1;
  const uint16_t SAMPLE1 = 0x1234;
  std::vector<uint8_t> work_buffer(BUFFER_SIZE);
  uint16_t value;

  // *** Test a buffer that is too small for even the header

  lbne::NanoSliceWriter ns_writer(&work_buffer[0], SMALL_SIZE1, CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE(! ns_writer.sampleValue(0, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(999, value));

  BOOST_REQUIRE(! ns_writer.addSample(SAMPLE1));
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE(! ns_writer.sampleValue(0, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(1, value));

  int32_t size_diff = ns_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, 0);
}

BOOST_AUTO_TEST_CASE(SmallBufferTest)
{
  const uint16_t CHANNEL_NUMBER = 456;
  const uint32_t BUFFER_SIZE = 4096;
  const uint32_t SMALL_SIZE2 = sizeof(lbne::NanoSlice::Header) + sizeof(uint16_t) + 1;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  std::vector<uint8_t> work_buffer(BUFFER_SIZE);
  uint16_t value;

  // *** Test a buffer that is too small for two values

  lbne::NanoSliceWriter ns_writer(&work_buffer[0], SMALL_SIZE2, CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE(! ns_writer.sampleValue(0, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(999, value));

  BOOST_REQUIRE(ns_writer.addSample(SAMPLE1));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 1);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(! ns_writer.sampleValue(1, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(999, value));

  BOOST_REQUIRE(! ns_writer.addSample(SAMPLE2));
  BOOST_REQUIRE_EQUAL(ns_writer.size(),
                      sizeof(lbne::NanoSlice::Header) + sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 1);
  BOOST_REQUIRE(ns_writer.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(! ns_writer.sampleValue(1, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(2, value));

  int32_t size_diff = ns_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, 1);
}

BOOST_AUTO_TEST_CASE(CopyTest)
{
  const uint16_t CHANNEL_NUMBER = 123;
  const uint32_t BUFFER_SIZE = 4096;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  const uint16_t SAMPLE3 = 0xbeef;
  const uint16_t SAMPLE4 = 0xfe87;
  const uint16_t SAMPLE5 = 0x5a5a;
  std::vector<uint8_t> work_buffer(BUFFER_SIZE);
  uint16_t value;

  lbne::NanoSliceWriter ns_writer(&work_buffer[0], BUFFER_SIZE, CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE1));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE2));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE3));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE4));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE5));

  int32_t size_diff = ns_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, BUFFER_SIZE -
                      sizeof(lbne::NanoSlice::Header) - 5*sizeof(uint16_t));

  lbne::NanoSliceWriter ns_writer_copy = ns_writer;
  BOOST_REQUIRE_EQUAL(ns_writer_copy.sampleCount(), 5);
  BOOST_REQUIRE(ns_writer_copy.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(ns_writer_copy.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(ns_writer_copy.sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(ns_writer_copy.sampleValue(3, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(ns_writer_copy.sampleValue(4, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE5);
  BOOST_REQUIRE(! ns_writer.addSample(SAMPLE5));

  lbne::NanoSlice nslice(&work_buffer[0]);
  lbne::NanoSlice nslice_copy = nslice;
  BOOST_REQUIRE_EQUAL(nslice_copy.size(),
                      sizeof(lbne::NanoSlice::Header) + 5*sizeof(uint16_t));
  BOOST_REQUIRE_EQUAL(nslice_copy.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(nslice_copy.sampleCount(), 5);
  BOOST_REQUIRE(nslice_copy.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE1);
  BOOST_REQUIRE(nslice_copy.sampleValue(1, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE2);
  BOOST_REQUIRE(nslice_copy.sampleValue(2, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(nslice_copy.sampleValue(3, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE4);
  BOOST_REQUIRE(nslice_copy.sampleValue(4, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE5);
  BOOST_REQUIRE(! nslice_copy.sampleValue(6, value));
  BOOST_REQUIRE(! nslice_copy.sampleValue(567, value));
}

BOOST_AUTO_TEST_CASE(BufferReuseTest)
{
  const uint16_t CHANNEL_NUMBER = 123;
  const uint32_t BUFFER_SIZE = 4096;
  const uint16_t SAMPLE1 = 0x1234;
  const uint16_t SAMPLE2 = 0xc3c3;
  const uint16_t SAMPLE3 = 0xbeef;
  std::vector<uint8_t> work_buffer(BUFFER_SIZE);
  uint16_t value;

  lbne::NanoSliceWriter ns_writer(&work_buffer[0], BUFFER_SIZE, CHANNEL_NUMBER);
  BOOST_REQUIRE_EQUAL(ns_writer.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer.channelNumber(), CHANNEL_NUMBER);
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE1));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE2));
  BOOST_REQUIRE(ns_writer.addSample(SAMPLE3));

  int32_t size_diff = ns_writer.finalize();
  BOOST_REQUIRE_EQUAL(size_diff, BUFFER_SIZE -
                      sizeof(lbne::NanoSlice::Header) - 3*sizeof(uint16_t));

  // *** If we reuse a buffer, then all history of the earlier NanoSlice
  // *** should be gone

  lbne::NanoSliceWriter ns_writer2(&work_buffer[0], BUFFER_SIZE, 0);
  BOOST_REQUIRE_EQUAL(ns_writer2.size(), sizeof(lbne::NanoSlice::Header));
  BOOST_REQUIRE_EQUAL(ns_writer2.sampleCount(), 0);
  BOOST_REQUIRE_EQUAL(ns_writer2.channelNumber(), 0);
  BOOST_REQUIRE(ns_writer2.addSample(SAMPLE3));
  BOOST_REQUIRE_EQUAL(ns_writer2.sampleCount(), 1);
  BOOST_REQUIRE(ns_writer2.sampleValue(0, value));
  BOOST_REQUIRE_EQUAL(value, SAMPLE3);
  BOOST_REQUIRE(! ns_writer2.sampleValue(1, value));
  BOOST_REQUIRE(! ns_writer2.sampleValue(2, value));

  // *** And, if we try to use the earlier Writer after the buffer has
  // *** been reused, that shouldn't work either

  BOOST_REQUIRE(! ns_writer.sampleValue(1, value));
  BOOST_REQUIRE(! ns_writer.sampleValue(2, value));
}

BOOST_AUTO_TEST_SUITE_END()
