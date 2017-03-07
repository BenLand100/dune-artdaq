#include "artdaq-core/ArtModules/BuildInfo_module.hh" 

#include "artdaq/BuildInfo/GetPackageBuildInfo.hh" 
#include "artdaq-core/BuildInfo/GetPackageBuildInfo.hh" 
#include "dune-raw-data/BuildInfo/GetPackageBuildInfo.hh" 
#include "dune-artdaq/BuildInfo/GetPackageBuildInfo.hh" 

#include <string>

namespace dune {

  static std::string instanceName = "DuneArtdaqBuildInfo";
  typedef artdaq::BuildInfo< &instanceName, artdaqcore::GetPackageBuildInfo, artdaq::GetPackageBuildInfo, dunerawdata::GetPackageBuildInfo, dune::GetPackageBuildInfo> DuneArtdaqBuildInfo;

  DEFINE_ART_MODULE(DuneArtdaqBuildInfo)
}
