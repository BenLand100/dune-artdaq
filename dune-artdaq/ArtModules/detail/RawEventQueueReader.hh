#ifndef dune_artdaq_ArtModules_detail_RawEventQueueReader_hh
#define dune_artdaq_ArtModules_detail_RawEventQueueReader_hh

#include <string>
#include <map>

#include "artdaq/ArtModules/detail/RawEventQueueReader.hh"

namespace dune {
  namespace detail {
    struct RawEventQueueReader : public artdaq::detail::RawEventQueueReader {
      RawEventQueueReader(RawEventQueueReader const &) = delete;
      RawEventQueueReader & operator=(RawEventQueueReader const &) = delete;

      RawEventQueueReader(fhicl::ParameterSet const & ps,
                          art::ProductRegistryHelper & help,
                          art::SourceHelper const & pm);

      RawEventQueueReader(fhicl::ParameterSet const & ps,
                          art::ProductRegistryHelper & help,
                          art::SourceHelper const & pm,
			  art::MasterProductRegistry&) : RawEventQueueReader(ps, help, pm) {}
    };

  } // detail
} // dune

// Specialize an art source trait to tell art that we don't care about
// source.fileNames and don't want the files services to be used.
namespace art {
  template <>
  struct Source_generator<dune::detail::RawEventQueueReader> {
    static constexpr bool value = true;
  };
}

#endif /* dune_artdaq_ArtModules_detail_RawEventQueueReader_hh */
