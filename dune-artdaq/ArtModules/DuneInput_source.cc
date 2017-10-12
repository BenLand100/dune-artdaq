#include "art/Framework/IO/Sources/Source.h"
#include "dune-artdaq/ArtModules/detail/RawEventQueueReader.hh"
#include "art/Framework/Core/InputSourceMacros.h"

#include <string>
using std::string;

namespace dune {

  typedef art::Source<detail::RawEventQueueReader> DuneInput;
}

DEFINE_ART_INPUT_SOURCE(dune::DuneInput)
