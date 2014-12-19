////////////////////////////////////////////////////////////////////////
// Class:       TriggerAna
// Module Type: analyzer
// File:        TriggerAna_module.cc
//
// Generated at Wed Dec 17 10:26:07 2014 by Matthew Tamsett using artmod
// from cetpkgsupport v1_07_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Services/Optional/TFileService.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "artdaq-core/Data/Fragments.hh"
#include <iostream>
#include "TH1.h"
#include "TFile.h"

class TriggerAna;

class TriggerAna : public art::EDAnalyzer {
public:
  explicit TriggerAna(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  TriggerAna(TriggerAna const &) = delete;
  TriggerAna(TriggerAna &&) = delete;
  TriggerAna & operator = (TriggerAna const &) = delete;
  TriggerAna & operator = (TriggerAna &&) = delete;

  // Required functions.
  void analyze(art::Event const & evt) override;

  // Selected optional functions.
  void beginJob() override;
  void endJob() override;

private:
  // Declare member data here.
  std::string _sspFragmentType = "PHOTON"; ///< SSP fragment type
  unsigned int _nEvents           = 0;
  unsigned int _nFragments        = 0;
  unsigned int _nEventsPassed     = 0;
  // FHiCL options
  std::string _triggerModuleLabel;  ///< Label of the trigger to require, if set to all then it we study all events
  std::string _sspModuleLabel;      ///< Label of the module making the SSP fragments
  bool _verbose;                    ///< Control verbosity of output  
  // Declare histograms
  TH1D  *h_ssp_adc;
};


TriggerAna::TriggerAna(fhicl::ParameterSet const & p)
  :
  EDAnalyzer(p)  // ,
 // More initializers here.
{
  _triggerModuleLabel = p.get<std::string>  ("TriggerModuleLabel",  "all");
  _sspModuleLabel     = p.get<std::string>  ("SspModuleLabel",      "daq");
  _verbose            = p.get<bool>         ("Verbose",             false);

  if (_verbose){
    std::cout << "--- TriggerAna " << std::endl;
    std::cout << "    trigger module label:       "   << _triggerModuleLabel<< std::endl;
    std::cout << "    SSP module label:           "   << _sspModuleLabel    << std::endl;
    std::cout << "    verbose:                    "   << _verbose           << std::endl;
  }
}

void TriggerAna::beginJob()
{
  art::ServiceHandle<art::TFileService> tfs;
  h_ssp_adc = tfs->make<TH1D>("ssp_adc_values","ssp_adc_values",120,-0.5,119.5);  
}

void TriggerAna::analyze(art::Event const & evt)
{
  // Implementation of required member function here.
  _nEvents++;
  bool trigger_decision = false;
  auto eventID = evt.id();

  if (_verbose) std::cout << "--- TriggerAna, eventID: " << eventID << std::endl;

  // Filter by trigger decision
  if (_triggerModuleLabel == "all"){
    trigger_decision = true;
  } else {
    art::Handle< bool > the_trigger_filter;
    evt.getByLabel(_triggerModuleLabel, "", the_trigger_filter);
    assert(the_trigger_filter.isValid());
    if (*the_trigger_filter) trigger_decision = true;
  }

  if (!trigger_decision) return;
  _nEventsPassed++;

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
      } else {
        std::cout << "        SSP fragment has no metadata associated with it." << std::endl;
      } // end of if on valid meta-data

      // Look at the ADC values
      const unsigned int* dataPointer = ssp_fragment.dataBegin();
      
      unsigned int triggersProcessed=0;
      while( (ssp_header == 0 || triggersProcessed<ssp_header->nTriggers) &&
              dataPointer<ssp_fragment.dataEnd())
      {
        const SSPDAQ::EventHeader* daqHeader=reinterpret_cast<const SSPDAQ::EventHeader*>(dataPointer);
        
        uint32_t peaksum = ((daqHeader->group3 & 0x00FF) >> 16) + daqHeader->peakSumLow;
        if(peaksum & 0x00800000) peaksum |= 0xFF000000;
        
        dataPointer+=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
        
        //get the information from the data
        unsigned int nADC=(daqHeader->length-sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int))*2;
        const unsigned short* adcPointer=reinterpret_cast<const unsigned short*>(dataPointer);

        for(size_t idata = 0; idata < nADC; idata++) {
          const unsigned short* adc = adcPointer + idata;
          h_ssp_adc->Fill(*adc);
        }
        dataPointer+=nADC/2;
        ++triggersProcessed;
      } // end of while over each trigger
    } // end of loop over SSP fragments
  } // end of if on is SSP fragment valid
}

void TriggerAna::endJob()
{
  std::cout << "--- TriggerAna endJob" << std::endl;
  std::cout << "    trigger module label:   "   << _triggerModuleLabel<< std::endl;
  std::cout << "    events:                 " << _nEvents         << std::endl;
  std::cout << "    fragments:              " << _nFragments      << std::endl;
  std::cout << "    events passed:          " << _nEventsPassed   << std::endl;
}

DEFINE_ART_MODULE(TriggerAna)
