#ifndef dune_artdaq_BuildInfo_GetPackageBuildInfo_hh
#define dune_artdaq_BuildInfo_GetPackageBuildInfo_hh

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <string>

namespace dune {

  struct GetPackageBuildInfo {

    static artdaq::PackageBuildInfo getPackageBuildInfo();
  };

}

#endif /* dune_artdaq_BuildInfo_GetPackageBuildInfo_hh */
