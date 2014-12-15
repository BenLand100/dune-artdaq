////////////////////////////////////////////////////////////////////////
// Class:       SSPTrigger
// Module Type: filter
// File:        SSPTrigger_module.cc
//
// Generated at Mon Dec 15 09:27:45 2014 by Matthew Tamsett using artmod
// from cetpkgsupport v1_07_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>

#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "artdaq-core/Data/Fragments.hh"
#include <iostream>

class SSPTrigger;

class SSPTrigger : public art::EDFilter {
public:
  explicit SSPTrigger(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  SSPTrigger(SSPTrigger const &) = delete;
  SSPTrigger(SSPTrigger &&) = delete;
  SSPTrigger & operator = (SSPTrigger const &) = delete;
  SSPTrigger & operator = (SSPTrigger &&) = delete;

  // Required functions.
  bool filter(art::Event & e) override;

  // Selected optional functions.
  // bool beginRun(art::Run & r) override;
  void endJob() override;

private:
  std::string _sspFragmentType = "PHOTON"; ///< SSP fragment type
  unsigned int _nEvents           = 0;
  unsigned int _nFragments        = 0;
  unsigned int _nFragmentsPassed  = 0;
  unsigned int _nEventsPassed     = 0;
  unsigned int _nSelectAll        = 0;
  // FHiCL options
  std::string _sspModuleLabel;  ///< Label of the module making the SSP fragments
  bool _cutOnNTriggers;         ///< Switch to turn on selection by the number of triggers
  unsigned int _minNTriggers;   ///< The minimum number of SSP triggers needed to select
  bool _selectAll;              ///< Select all events
  bool _verbose;                ///< Control verbosity of output
};


SSPTrigger::SSPTrigger(fhicl::ParameterSet const & p)
// :
// Initialize member data here.
{
  _sspModuleLabel = p.get<std::string>  ("SspModuleLabel",  "daq");
  _cutOnNTriggers = p.get<bool>         ("CutOnNTriggers",  false);
  _minNTriggers   = p.get<unsigned int> ("MinNTriggers",    0);
  _selectAll      = p.get<bool>         ("SelectAll",       false);
  _verbose        = p.get<bool>         ("Verbose",         false);

  if (_verbose){
    std::cout << "--- SSP trigger " << std::endl;
    std::cout << "    SSP module label:           "   << _sspModuleLabel  << std::endl;
    std::cout << "    Cut on number of triggers:  "   << _cutOnNTriggers  << std::endl;
    std::cout << "      - minimum:                "   << _minNTriggers    << std::endl;
    std::cout << "    Select all:                 "   << _selectAll       << std::endl;
    std::cout << "    verbose:                    "   << _verbose         << std::endl;
  }
}

bool SSPTrigger::filter(art::Event & evt)
{
  _nEvents++;
  bool trigger_decision = false;

  auto eventID = evt.id();
  if (_verbose){
   std::cout << "--- SSP trigger, eventID: " << eventID << std::endl;
  }

  // Look at the SSP data
  art::Handle<artdaq::Fragments> ssp_fragments;
  evt.getByLabel(_sspModuleLabel, _sspFragmentType, ssp_fragments);
  
  if (ssp_fragments.isValid()) {
    if (_verbose){
      std::cout << "    found: " << ssp_fragments->size()
                << " fragment(s) of type " << _sspFragmentType << std::endl;
    }
    
    for (size_t idx = 0; idx < ssp_fragments->size(); ++idx) {
      _nFragments++;
      const auto& fragment((*ssp_fragments)[idx]);      
      lbne::SSPFragment ssp_fragment(fragment);
      
      if (_verbose){
        std::cout << "        SSP fragment "  << fragment.fragmentID() 
                  << " has total size: "      << ssp_fragment.hdr_event_size()
                  << " and run number: "      << ssp_fragment.hdr_run_number()
                  << " with "                 << ssp_fragment.total_adc_values() 
                  << " total ADC values"      << std::endl;
      }

      const SSPDAQ::MillisliceHeader* ssp_header=0;

      if(fragment.hasMetadata()) {
        ssp_header = &(fragment.metadata<lbne::SSPFragment::Metadata>()->sliceHeader);
        
        if (_verbose){
          std::cout << "        SSP metadata: "
                    << "Start time: "           << ssp_header->startTime
                    << ", End time: "           << ssp_header->endTime
                    << ", Packet length: "      << ssp_header->length
                    << ", Number of triggers: " << ssp_header->nTriggers
                    << std::endl;
        }
        if (_cutOnNTriggers && (ssp_header->nTriggers >= _minNTriggers)){
          _nFragmentsPassed++;
          trigger_decision = true;
          if (_verbose) std::cout << "        --- Trigger passed" << std::endl;
        }
      } else {
        std::cout << "        SSP fragment has no metadata associated with it." << std::endl;
      } // end of if on valid meta-data
    } // end of loop over SSP fragments
  } // end of if on is SSP fragment valid

  if (_verbose) std::cout << "    Trigger decision: " << trigger_decision << std::endl;
  
  if (_selectAll){
    _nSelectAll++;
    trigger_decision = true;
    if (_verbose) std::cout << "      -- selecting all events" << std::endl;
  }

  if (trigger_decision) _nEventsPassed++;
  return trigger_decision;
}

// bool SSPTrigger::beginRun(art::Run & r)
// {
//   std::cout << "--- SSP trigger beginRun, run: " << r << std::endl;
// }

void SSPTrigger::endJob()
{
  std::cout << "--- SSP trigger endJob" << std::endl;
  std::cout << "    events:                 " << _nEvents         << std::endl;
  std::cout << "    fragments:              " << _nFragments      << std::endl;
  std::cout << "    fragments passed:       " << _nFragmentsPassed<< std::endl;
  std::cout << "    events passed:          " << _nEventsPassed   << std::endl;
  std::cout << "    events passed through:  " << _nSelectAll      << std::endl;
}

DEFINE_ART_MODULE(SSPTrigger)
