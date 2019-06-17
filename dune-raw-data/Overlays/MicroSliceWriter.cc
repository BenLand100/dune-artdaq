#include "dune-raw-data/Overlays/MicroSliceWriter.hh"
#include "cetlib/exception.h"

dune::MicroSliceWriter::MicroSliceWriter(uint8_t* address, uint32_t max_size_bytes) :
  MicroSlice(address), max_size_bytes_(max_size_bytes)
{
  header_()->version = 1;
  header_()->microslice_size = sizeof(Header);
  header_()->nanoslice_count = 0;
}

std::shared_ptr<dune::NanoSliceWriter>
dune::MicroSliceWriter::reserveNanoSlice(uint32_t ns_max_bytes)
{
  // finalize the most recent NanoSlice, in case that hasn't
  // already been done
  finalizeLatestNanoSlice_();

  // test if this new NanoSlice could overflow our maximum size
  if ((size() + ns_max_bytes) > max_size_bytes_) {
    throw cet::exception("MicroSliceWriter") << "Slice overflow error";
  }

  // create the next NanoSlice in our buffer, and update our
  // counters to include the new NanoSlice
  uint8_t* ns_ptr = data_(header_()->nanoslice_count);
  latest_nanoslice_ptr_.reset(new NanoSliceWriter(ns_ptr, ns_max_bytes));
  ++(header_()->nanoslice_count);
  header_()->microslice_size += ns_max_bytes;
  return latest_nanoslice_ptr_;
}

int32_t dune::MicroSliceWriter::finalize()
{
  // first, we need to finalize the last NanoSlice, in case that
  // hasn't already been done
  finalizeLatestNanoSlice_();

  // next, we update our maximum size so that no more NanoSlices
  // can be added
  int32_t size_diff = max_size_bytes_ - header_()->microslice_size;
  max_size_bytes_ = header_()->microslice_size;
  return size_diff;
}

void dune::MicroSliceWriter::finalizeLatestNanoSlice_()
{
  if (header_()->nanoslice_count > 0 &&
      latest_nanoslice_ptr_.get() != 0) {
    int size_change = latest_nanoslice_ptr_->finalize();
    header_()->microslice_size -= size_change;
  }
}

dune::MicroSlice::Header* dune::MicroSliceWriter::header_()
{
  return reinterpret_cast<Header *>(buffer_);
}
