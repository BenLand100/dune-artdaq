#ifndef dune_artdaq_Overlays_TpcMilliSliceFragment_hh
#define dune_artdaq_Overlays_TpcMilliSliceFragment_hh

#include "dune-raw-data/Overlays/TpcMilliSlice.hh"
#include "dune-raw-data/Overlays/TpcMicroSlice.hh"
#include "artdaq-core/Data/Fragment.hh"

namespace dune {
  class TpcMilliSliceFragment;
}

class dune::TpcMilliSliceFragment : public dune::TpcMilliSlice {

public:

  // This constructor accepts an artdaq::Fragment that contains an existing
  // TpcMilliSlice and allows the the data inside the TpcMilliSlice to be accessed
  TpcMilliSliceFragment(artdaq::Fragment const& frag);

protected:

  // returns a pointer to the header
  Header const* header_() const;

  // returns a pointer to the requested MicroSlice
  uint8_t* data_(int index) const;

private:

  artdaq::Fragment const& artdaq_fragment_;
};

#endif /* dune_artdaq_Overlays_TpcMilliSliceFragment_hh */
