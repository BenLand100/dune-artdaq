////////////////////////////////////////////////////////////////////////
// Class:       DUNEMetadata
// Plugin Type: filecatalogmetadataplugin (art v2_08_04)
// File:        DUNEMetadata_plugin.cc
//
// Generated at Fri Jan 19 15:37:11 2018 by Jeremy Hewes using cetskelgen
// from cetlib version v3_01_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/FileCatalogMetadataPlugin.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/SubRun.h"
#include "fhiclcpp/ParameterSetRegistry.h"
#include "dune-artdaq/Metadata/MetadataManager.h"

#include <sstream>

namespace meta {

  class DUNEMetadata;

  class DUNEMetadata : public art::FileCatalogMetadataPlugin {
  public:
    using collection_type = std::vector<std::pair<std::string, std::string>>;

    explicit DUNEMetadata(fhicl::ParameterSet const & p);

    DUNEMetadata(DUNEMetadata const &) = delete;
    DUNEMetadata(DUNEMetadata &&) = delete;
    DUNEMetadata & operator = (DUNEMetadata const &) = delete;
    DUNEMetadata & operator = (DUNEMetadata &&) = delete;

    auto produceMetadata() -> collection_type override;
    void fillMetadata();

    void beginJob();
    void collectMetadata(art::Event const & e);
    void beginSubRun(art::SubRun const & sr);

  private:

    std::string fFileName;

    std::string fCampaign;
    std::string fRunType;
    std::string fFileType;
    std::string fDataStream;
    std::string fDataTier;

    std::string fApplication;
    std::string fApplicationFamily;
    std::string fApplicationVersion;

    std::string fPartition;

    int         fNEvts;
    int         fFirstEvent;
    int         fLastEvent;
    std::string fStartTime;
    std::string fEndTime;

    bool        fIsRealData;

    std::set<std::pair<int,int>> fSubRuns;
    std::vector<std::string> fParents;

  }; // class DUNEMetadata


  DUNEMetadata::DUNEMetadata(fhicl::ParameterSet const & p)
    :
    FileCatalogMetadataPlugin(p),

    fCampaign   (p.get< std::string >("campaign"       ,"undefined")),
    fRunType    (p.get< std::string >("run_type"       ,"undefined")),
    fFileType   (p.get< std::string >("file_type"      ,"undefined")),
    fDataStream (p.get< std::string >("data_stream"    ,"undefined")),
    fDataTier   (p.get< std::string >("data_tier"      ,"undefined")),
    fPartition  (p.get< std::string >("local_partition","undefined")),
    fNEvts      (0),
    fFirstEvent (-1)
  {
    MetadataManager & manager = MetadataManager::getInstance();

    fhicl::ParameterSet inPSet;
    bool foundPSet = p.get_if_present<fhicl::ParameterSet>("params",inPSet);
    if (foundPSet) manager.DoParsing(inPSet);

  } // function DUNEMetadata::DUNEMetadata

  auto DUNEMetadata::produceMetadata() -> collection_type
  {
    MetadataManager & manager = MetadataManager::getInstance();

    fillMetadata();

    auto md = manager.GetMetadata();
    std::vector<std::pair<std::string,std::string>> v;
    v.reserve(md.size());
    for (auto const& it : md) {
      v.push_back(std::make_pair(it.first,it.second));
    }

    return v;

  } // function DUNEMetadata::produceMetadata

  void DUNEMetadata::fillMetadata()
  {
    MetadataManager & manager = MetadataManager::getInstance();

    // Initialise output value stream
    std::ostringstream value_stream;

    // Run type metadata
    manager.AddMetadata("campaign",fCampaign);
    manager.AddMetadata("file_type",fFileType);
    manager.AddMetadata("file_format","artroot");
    manager.AddMetadata("data_stream",fDataStream);
    manager.AddMetadata("data_tier",fDataTier);

    // Event numbers metadata
    value_stream << fNEvts;
    manager.AddMetadata("event_count",value_stream.str());
    value_stream.str("");

    value_stream << fFirstEvent;
    manager.AddMetadata("first_event",value_stream.str());
    value_stream.str("");

    value_stream << fLastEvent;
    manager.AddMetadata("last_event",value_stream.str());
    value_stream.str("");

    // Local environment info
    manager.AddMetadata("local_partition",fPartition);

    // Add list of subruns...
    value_stream << "[";
    for (auto it = fSubRuns.begin(); it != fSubRuns.end(); ++it) {
      if (it != fSubRuns.begin()) value_stream << ",\n";
        value_stream << "[ " << it->first << " , "
        << it->second << " , \"" << fRunType << "\"]";
    }
    value_stream << "]";
    manager.AddMetadata("runs",value_stream.str());

  } // function DUNEMetadata::fillMetadata

  void DUNEMetadata::beginJob()
  {

    std::cout << "Beginning job! Let's try accessing some FHICL parameter sets." << std::endl;
    auto psets = fhicl::ParameterSetRegistry::get();
    for (auto it = psets.begin(); it != psets.end(); ++it) {
      std::cout << "Contents of this parameter set is:" << std::endl;
      std::cout << it->second.to_string() << std::endl << std::endl;
    }

  } // function DUNEMetadata::beginJob

  void DUNEMetadata::collectMetadata(art::Event const & e)
  {
    if (fFirstEvent == -1) fFirstEvent = e.event();
    fLastEvent = e.event();
    ++fNEvts;

  } // function DUNEMetadata::collectMetadata

  void DUNEMetadata::beginSubRun(art::SubRun const & sr)
  {
    fSubRuns.insert(std::make_pair(sr.run(),sr.subRun()));

  } // function DUNEMetadata::beginSubRun

  DEFINE_ART_FILECATALOGMETADATA_PLUGIN(DUNEMetadata)

} // namespace meta

