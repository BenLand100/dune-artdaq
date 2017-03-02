////////////////////////////////////////////////////////////////////////
// Class:       LbneArtdaqPrintVersionInfo
// Module Type: analyzer
// File:        LbneArtdaqPrintVersionInfo_module.cc
//
// Generated at Fri Aug 15 21:05:07 2014 by dunedaq using artmod
// from cetpkgsupport v1_05_02.
////////////////////////////////////////////////////////////////////////


// JCF, 8/19/14

// As the following function:

// void analyze(art::Event const & e) 

// in LbneArtdaqPrintVersionInfo's base class, art::EDAnalyzer, needs
// to be overridden, but this module only analyzes run-by-run info
// rather than event-by-event info, there's no point in adding an
// implementation body to the function. Thus, for this module we
// disable the error condition usually called by an unused function
// parameter (here, the reference-to-art::Event)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <iostream>

namespace dune {
  class LbneArtdaqPrintVersionInfo;
}

class dune::LbneArtdaqPrintVersionInfo : public art::EDAnalyzer {
public:
  explicit LbneArtdaqPrintVersionInfo(fhicl::ParameterSet const & p);
  virtual ~LbneArtdaqPrintVersionInfo();

  void analyze(art::Event const & e) override { 
  }

  virtual void beginRun(art::Run const& r);


private:

  std::string dune_artdaq_module_label_;
  std::string dune_artdaq_instance_label_;

  std::string lbne_raw_data_module_label_;
  std::string lbne_raw_data_instance_label_;

};


dune::LbneArtdaqPrintVersionInfo::LbneArtdaqPrintVersionInfo(fhicl::ParameterSet const & pset)
  :
  EDAnalyzer(pset),
  dune_artdaq_module_label_(pset.get<std::string>("dune_artdaq_module_label")),
  dune_artdaq_instance_label_(pset.get<std::string>("dune_artdaq_instance_label")),
  lbne_raw_data_module_label_(pset.get<std::string>("lbne_raw_data_module_label")),
  lbne_raw_data_instance_label_(pset.get<std::string>("lbne_raw_data_instance_label"))
{}

dune::LbneArtdaqPrintVersionInfo::~LbneArtdaqPrintVersionInfo()
{
  // Clean up dynamic memory and other resources here.
}


void dune::LbneArtdaqPrintVersionInfo::beginRun(art::Run const& run)
{

  art::Handle<artdaq::PackageBuildInfo> raw;


  run.getByLabel(lbne_raw_data_module_label_, lbne_raw_data_instance_label_, raw);

  if (raw.isValid()) {
    std::cout << std::endl;
    std::cout << "lbne-raw-data package version: " << raw->getPackageVersion() << std::endl;
    std::cout << "lbne-raw-data package build time: " << raw->getBuildTimestamp() << std::endl;
    std::cout << std::endl;
  } else {
    std::cerr << std::endl;
    std::cerr << "Warning in LbneArtdaqPrintVersionInfo module: Run " << run.run() << " appears to have no info on lbne-raw-data build time / version number" << std::endl;
    std::cerr << std::endl;
  }


  run.getByLabel(dune_artdaq_module_label_, dune_artdaq_instance_label_, raw);

  if (raw.isValid()) {
    std::cout << std::endl;
    std::cout << "dune-artdaq package version: " << raw->getPackageVersion() << std::endl;
    std::cout << "dune-artdaq package build time: " << raw->getBuildTimestamp() << std::endl;
    std::cout << std::endl;
  } else {
    std::cerr << std::endl;
    std::cerr << "Warning in LbneArtdaqPrintVersionInfo module: Run " << run.run() << " appears to have no info on dune-artdaq build time / version number" << std::endl;
    std::cerr << std::endl;
  }

}

#pragma GCC diagnostic pop

DEFINE_ART_MODULE(dune::LbneArtdaqPrintVersionInfo)
