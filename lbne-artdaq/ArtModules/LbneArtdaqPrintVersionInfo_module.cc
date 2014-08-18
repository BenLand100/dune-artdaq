////////////////////////////////////////////////////////////////////////
// Class:       LbneArtdaqPrintVersionInfo
// Module Type: analyzer
// File:        LbneArtdaqPrintVersionInfo_module.cc
//
// Generated at Fri Aug 15 21:05:07 2014 by lbnedaq using artmod
// from cetpkgsupport v1_05_02.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <iostream>

namespace lbne {
  class LbneArtdaqPrintVersionInfo;
}

class lbne::LbneArtdaqPrintVersionInfo : public art::EDAnalyzer {
public:
  explicit LbneArtdaqPrintVersionInfo(fhicl::ParameterSet const & p);
  virtual ~LbneArtdaqPrintVersionInfo();

  void analyze(art::Event const & e) override { 
    if (e.run() == 99999999) {
      std::cout << "JCF, 8/16/16 -- the only reason the analyze() function in LbneArtdaqPrintVersionInfo has a body is because the compiler complains if the art::Event argument goes unused" << std::endl;
    }
  }

  virtual void beginRun(art::Run const& r);


private:

  std::string lbne_artdaq_module_label_;
  std::string lbne_artdaq_instance_label_;

  std::string lbne_raw_data_module_label_;
  std::string lbne_raw_data_instance_label_;

};


lbne::LbneArtdaqPrintVersionInfo::LbneArtdaqPrintVersionInfo(fhicl::ParameterSet const & pset)
  :
  EDAnalyzer(pset),
  lbne_artdaq_module_label_(pset.get<std::string>("lbne_artdaq_module_label")),
  lbne_artdaq_instance_label_(pset.get<std::string>("lbne_artdaq_instance_label")),
  lbne_raw_data_module_label_(pset.get<std::string>("lbne_raw_data_module_label")),
  lbne_raw_data_instance_label_(pset.get<std::string>("lbne_raw_data_instance_label"))
{}

lbne::LbneArtdaqPrintVersionInfo::~LbneArtdaqPrintVersionInfo()
{
  // Clean up dynamic memory and other resources here.
}


void lbne::LbneArtdaqPrintVersionInfo::beginRun(art::Run const& run)
{

  art::Handle<artdaq::PackageBuildInfo> raw;


  run.getByLabel(lbne_raw_data_module_label_, lbne_raw_data_instance_label_, raw);

  if (raw.isValid()) {
    std::cout << "lbne-raw-data package version: " << raw->getPackageVersion() << std::endl;
    std::cout << "lbne-raw-data package build time: " << raw->getBuildTimestamp() << std::endl;

  } else {
    std::cout << "Run " << run.run() << " appears to have no info on lbne-raw-data build time / version number" << std::endl;
  }


  run.getByLabel(lbne_artdaq_module_label_, lbne_artdaq_instance_label_, raw);

  if (raw.isValid()) {
    std::cout << "lbne-artdaq package version: " << raw->getPackageVersion() << std::endl;
    std::cout << "lbne-artdaq package build time: " << raw->getBuildTimestamp() << std::endl;

  } else {
    std::cout << "Run " << run.run() << " appears to have no info on lbne-artdaq build time / version number" << std::endl;
  }

}

DEFINE_ART_MODULE(lbne::LbneArtdaqPrintVersionInfo)
