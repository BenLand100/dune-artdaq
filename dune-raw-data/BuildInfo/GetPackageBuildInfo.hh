#ifndef dune_raw_data_BuildInfo_GetPackageBuildInfo_hh
#define dune_raw_data_BuildInfo_GetPackageBuildInfo_hh

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <string>

namespace dunerawdata {

  struct GetPackageBuildInfo {

    static artdaq::PackageBuildInfo getPackageBuildInfo();
  };

}

#endif /* dune_raw_data_BuildInfo_GetPackageBuildInfo_hh */
