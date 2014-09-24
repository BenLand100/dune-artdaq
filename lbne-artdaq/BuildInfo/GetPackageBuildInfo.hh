#ifndef lbne_artdaq_BuildInfo_GetPackageBuildInfo_hh
#define lbne_artdaq_BuildInfo_GetPackageBuildInfo_hh

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <string>

namespace lbne {

  struct GetPackageBuildInfo {

    static artdaq::PackageBuildInfo getPackageBuildInfo();
  };

}

#endif /* lbne_artdaq_BuildInfo_GetPackageBuildInfo_hh */
