#include "dune-raw-data/Overlays/MilliSlice.hh"

dune::MilliSlice::MilliSlice(uint8_t* address) : buffer_(address)
{
}

dune::MilliSlice::Header::millislice_size_t dune::MilliSlice::size() const
{
  return header_()->millislice_size;
}

dune::MilliSlice::Header::microslice_count_t dune::MilliSlice::microSliceCount() const
{
  return header_()->microslice_count;
}

std::unique_ptr<dune::MicroSlice> dune::MilliSlice::microSlice(uint32_t index) const
{
  std::unique_ptr<MicroSlice> mslice_ptr;
  if (index < microSliceCount()) {
    uint8_t* ms_ptr = data_(index);
    mslice_ptr.reset(new MicroSlice(ms_ptr));
  }
  return mslice_ptr;
}

dune::MilliSlice::Header const* dune::MilliSlice::header_() const
{
  return reinterpret_cast<Header const*>(buffer_);
}

uint8_t* dune::MilliSlice::data_(int index) const
{
  uint8_t* ms_ptr = buffer_ + sizeof(Header);
  for (int idx = 0; idx < index; ++idx) {
    MicroSlice tmp_ms(ms_ptr);
    ms_ptr += tmp_ms.size();
  }
  return ms_ptr;
}
