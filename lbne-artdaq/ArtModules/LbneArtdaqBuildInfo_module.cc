#include "artdaq-core/ArtModules/BuildInfo_module.hh" 

#include "artdaq/BuildInfo/GetPackageBuildInfo.hh" 
#include "artdaq-core/BuildInfo/GetPackageBuildInfo.hh" 
#include "lbne-raw-data/BuildInfo/GetPackageBuildInfo.hh" 
#include "lbne-artdaq/BuildInfo/GetPackageBuildInfo.hh" 

#include <string>

namespace lbne {

  static std::string instanceName = "LbneArtdaqBuildInfo";
  typedef artdaq::BuildInfo< &instanceName, artdaqcore::GetPackageBuildInfo, artdaq::GetPackageBuildInfo, lbnerawdata::GetPackageBuildInfo, lbne::GetPackageBuildInfo> LbneArtdaqBuildInfo;

  DEFINE_ART_MODULE(LbneArtdaqBuildInfo)
}
