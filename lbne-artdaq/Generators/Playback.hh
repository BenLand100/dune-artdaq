#ifndef lbne_artdaq_Generators_Playback_hh
#define lbne_artdaq_Generators_Playback_hh


#include "lbne-raw-data/Overlays/FragmentType.hh"

#include "artdaq-core/Data/Fragments.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "art/Framework/Core/RootDictionaryManager.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Utilities/Exception.h"
#include "art/Framework/Core/FileBlock.h"
#include "art/Framework/Core/InputSourceMacros.h"
#include "art/Framework/Core/ProductRegistryHelper.h"
#include "art/Framework/IO/Root/rootNames.h"
#include "art/Framework/IO/Sources/Source.h"
#include "art/Framework/IO/Sources/SourceHelper.h"
#include "art/Framework/IO/Sources/SourceTraits.h"
#include "art/Framework/IO/Sources/put_product_in_principal.h"
#include "art/Framework/Principal/EventPrincipal.h"
#include "art/Framework/Principal/RunPrincipal.h"
#include "art/Framework/Principal/SubRunPrincipal.h"
#include "art/Persistency/Provenance/EventID.h"
#include "art/Persistency/Provenance/MasterProductRegistry.h"
#include "art/Persistency/Provenance/RunID.h"
#include "art/Persistency/Provenance/SubRunID.h"
#include "art/Utilities/InputTag.h"


#include "fhiclcpp/fwd.h"

#include "TFile.h"
#include "TTree.h"
#include "TBranch.h"

#include <vector>
#include <string>

namespace lbne {    

  class Playback : public artdaq::CommandableFragmentGenerator {
  public:
    explicit Playback(fhicl::ParameterSet const & ps);
    ~Playback() = default;

    using FT = detail::FragmentType;

  private:

    bool getNext_(artdaq::FragmentPtrs & output) override;

    void start() override {}
    void stop() override {}
    void stopNoMutex() override {}

    template <typename PROD>
    const char * getBranchNameAsCStr(art::InputTag const& tag) const;

    FT fragment_type_;

    // JCF, Jun-3-2016

    // Using raw pointers rather than smart pointers b/c (I believe)
    // we won't have ownership of the objects pointed to

    TFile* file_;
    TTree* tree_;
    TBranch* branch_;
    std::vector<TBranch*> branches_;

    // JCF, Jun-3-2016

    // An instance of the RootDictionaryManager is necessary in order
    // to read objects out of art's saved *.root files

    art::RootDictionaryManager dictionary_loader_;

    // Not declared static as I'd prefer this initialized in the *.hh file...

    const std::vector<FT> possible_fragment_types_ = { 
      FT::TOY1, FT::TOY2, FT::TPC, FT::PHOTON, FT::TRIGGER };
  };

  template <typename PROD>
  const char * Playback::getBranchNameAsCStr(art::InputTag const& tag) const
  {
    std::ostringstream oss;
    oss << art::TypeID(typeid(PROD)).friendlyClassName()
	<< '_'
	<< tag.label()
	<< '_'
	<< tag.instance()
	<< '_'
	<< tag.process()
	<< ".obj";
    return oss.str().data();
  }

}

#endif /* lbne_artdaq_Generators_Playback_hh */
