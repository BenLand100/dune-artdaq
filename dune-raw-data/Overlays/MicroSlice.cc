#include "dune-raw-data/Overlays/MicroSlice.hh"

dune::MicroSlice::MicroSlice(uint8_t* address) : buffer_(address)
{
}

dune::MicroSlice::Header::microslice_size_t dune::MicroSlice::size() const
{
  return header_()->microslice_size;
}

dune::MicroSlice::Header::nanoslice_count_t dune::MicroSlice::nanoSliceCount() const
{
  return header_()->nanoslice_count;
}

std::unique_ptr<dune::NanoSlice> dune::MicroSlice::nanoSlice(uint32_t index) const
{
  std::unique_ptr<NanoSlice> nslice_ptr;
  if (index < nanoSliceCount()) {
    uint8_t* ns_ptr = data_(index);
    nslice_ptr.reset(new NanoSlice(ns_ptr));
  }
  return nslice_ptr;
}

dune::MicroSlice::Header const* dune::MicroSlice::header_() const
{
  return reinterpret_cast<Header const *>(buffer_);
}

uint8_t* dune::MicroSlice::data_(uint32_t index) const
{
  uint8_t* ns_ptr = reinterpret_cast<uint8_t *>(buffer_ + sizeof(Header));
  for (uint32_t idx = 0; idx < index; ++idx) {
    NanoSlice tmp_ns(ns_ptr);
    ns_ptr += tmp_ns.size();
  }
  return ns_ptr;
}
