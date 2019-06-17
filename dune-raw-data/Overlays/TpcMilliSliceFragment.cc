#include "dune-raw-data/Overlays/TpcMilliSliceFragment.hh"

dune::TpcMilliSliceFragment::
TpcMilliSliceFragment(artdaq::Fragment const& frag) :
  TpcMilliSlice(reinterpret_cast<uint8_t*>(const_cast<artdaq::Fragment::byte_t*>(frag.dataBeginBytes()))),
  artdaq_fragment_(frag)
{
}

dune::TpcMilliSliceFragment::Header const* dune::TpcMilliSliceFragment::header_() const
{
  return reinterpret_cast<Header const*>(artdaq_fragment_.dataBeginBytes());
}

uint8_t* dune::TpcMilliSliceFragment::data_(int index) const
{
  uint8_t* ms_ptr = reinterpret_cast<uint8_t*>(const_cast<artdaq::Fragment::byte_t*>(artdaq_fragment_.dataBeginBytes()))
    + sizeof(Header);
  for (int idx = 0; idx < index; ++idx) {
    TpcMicroSlice tmp_ms(ms_ptr);
    ms_ptr += tmp_ms.size();
  }
  return ms_ptr;
}
