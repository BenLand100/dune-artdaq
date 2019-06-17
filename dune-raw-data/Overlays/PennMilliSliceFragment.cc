#include "dune-raw-data/Overlays/PennMilliSliceFragment.hh"

dune::PennMilliSliceFragment::
PennMilliSliceFragment(artdaq::Fragment const& frag) :
  PennMilliSlice(reinterpret_cast<uint8_t*>(const_cast<artdaq::Fragment::byte_t*>(frag.dataBeginBytes()))),
  artdaq_fragment_(frag)
{
}

dune::PennMilliSliceFragment::Header const* dune::PennMilliSliceFragment::header_() const
{
  return reinterpret_cast<Header const*>(artdaq_fragment_.dataBeginBytes());
}

uint8_t* dune::PennMilliSliceFragment::data_(int index) const
{
  uint8_t* ms_ptr = reinterpret_cast<uint8_t*>(const_cast<artdaq::Fragment::byte_t*>(artdaq_fragment_.dataBeginBytes()))
    + sizeof(Header);
  for (int idx = 0; idx < index; ++idx) {
    PennMicroSlice tmp_ms(ms_ptr);
    ms_ptr += tmp_ms.size();
  }
  return ms_ptr;
}
