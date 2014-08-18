#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Run.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "artdaq-core/Data/PackageBuildInfo.hh"
#include "lbne-artdaq/Version/GetReleaseVersion.h"
#include "lbne-raw-data/Version/GetReleaseVersion.h"

namespace lbne {

  class LbneArtdaqBuildInfo : public art::EDProducer {
  public:
    explicit LbneArtdaqBuildInfo(fhicl::ParameterSet const & p);
    virtual ~LbneArtdaqBuildInfo() {}

    void beginRun(art::Run & r) override;
    void produce(art::Event & e) override;

  private:
    std::string const instance_name_lbne_artdaq_;
    std::string const instance_name_lbne_raw_data_;
    art::RunNumber_t current_run_;
  };

  LbneArtdaqBuildInfo::LbneArtdaqBuildInfo(fhicl::ParameterSet const &p):
    instance_name_lbne_artdaq_(p.get<std::string>("instance_name_lbne_artdaq", "buildinfoLbneArtdaq")),
    instance_name_lbne_raw_data_(p.get<std::string>("instance_name_lbne_raw_data", "buildinfoLbneRawData")),
    current_run_(0)
  {
    produces<artdaq::PackageBuildInfo, art::InRun>(instance_name_lbne_artdaq_);
    produces<artdaq::PackageBuildInfo, art::InRun>(instance_name_lbne_raw_data_);
  }

  void LbneArtdaqBuildInfo::beginRun(art::Run &e) { 
    if (e.run () == current_run_) return;
    current_run_ = e.run ();

    std::string s1, s2;

    // create a PackageBuildInfo object for lbne-artdaq and add it to the Run
    std::unique_ptr<artdaq::PackageBuildInfo> build_info_lbne_artdaq(new artdaq::PackageBuildInfo());
    s1 = lbne::getReleaseVersion();
    build_info_lbne_artdaq->setPackageVersion(s1);
    s2 = lbne::getBuildDateTime();
    build_info_lbne_artdaq->setBuildTimestamp(s2);

    e.put(std::move(build_info_lbne_artdaq),instance_name_lbne_artdaq_);

    // And do the same for the artdaq-core package on which it depends

    std::unique_ptr<artdaq::PackageBuildInfo> build_info_lbne_raw_data(new artdaq::PackageBuildInfo());
    s1 = lbnerawdata::getReleaseVersion();
    build_info_lbne_raw_data->setPackageVersion(s1);
    s2 = lbnerawdata::getBuildDateTime();
    build_info_lbne_raw_data->setBuildTimestamp(s2);

    e.put(std::move(build_info_lbne_raw_data),instance_name_lbne_raw_data_);

  }

  void LbneArtdaqBuildInfo::produce(art::Event &)
  {
    // nothing to be done for individual events
  }

  DEFINE_ART_MODULE(LbneArtdaqBuildInfo)
}
