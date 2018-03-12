#include "art/Framework/IO/Sources/Source.h"
#include "artdaq/ArtModules/detail/SharedMemoryReader.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "art/Framework/Core/InputSourceMacros.h"
#include "art/Framework/IO/Sources/SourceTraits.h"

#include <string>
using std::string;

namespace dune {

  typedef art::Source< artdaq::detail::SharedMemoryReader<dune::makeFragmentTypeMap> > DuneInput;
}

namespace art
{
  template <>
  struct Source_generator<artdaq::detail::SharedMemoryReader<dune::makeFragmentTypeMap>>
  {
    static constexpr bool value = true; ///< Used to suppress use of file services on art Source     
  };
}


DEFINE_ART_INPUT_SOURCE(dune::DuneInput)
