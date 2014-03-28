#include "lbne-artdaq/Overlays/MilliSliceFragment.hh"

lbne::MilliSliceFragment::
MilliSliceFragment(artdaq::Fragment const& frag) :
  MilliSlice(reinterpret_cast<uint8_t*>(const_cast<artdaq::RawDataType*>(&*frag.dataBegin()))),
  artdaq_fragment_(frag)
{
}

lbne::MilliSliceFragment::Header const* lbne::MilliSliceFragment::header_() const
{
  return reinterpret_cast<Header const*>(&*artdaq_fragment_.dataBegin());
}

uint8_t* lbne::MilliSliceFragment::data_(int index) const
{
  uint8_t* ms_ptr = reinterpret_cast<uint8_t*>(const_cast<artdaq::RawDataType*>(&*artdaq_fragment_.dataBegin()))
    + sizeof(Header);
  for (int idx = 0; idx < index; ++idx) {
    MicroSlice tmp_ms(ms_ptr);
    ms_ptr += tmp_ms.size();
  }
  return ms_ptr;
}
