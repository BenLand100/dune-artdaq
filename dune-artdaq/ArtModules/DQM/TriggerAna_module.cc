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
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"


#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "artdaq-core/Data/Fragments.hh"
#include <iostream>
#include <climits>
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
  TH1D  *h_delta_event_number;
  art::EventNumber_t   _last_event_number;


  int _number_of_triggers;
  int _number_of_trggers_per_fragment;
  size_t _number_of_ssps;

  bool _make_per_fragment_histos=false;

  TH1D *h_number_of_triggers;

  TH1D *h_integrated_sum;
  TH1D *h_peak_time;
  TH1D *h_peak_sum;
  TH1D *h_channel_id;
  TH1D *h_trigger_id;
  TH1D *h_trigger_type;

  TH1D **h_number_of_triggers_per_fragment;

  TH1D **h_trigger_type_per_fragment;
  TH1D **h_trigger_id_per_fragment; 
  TH1D **h_channel_id_per_fragment; 
  TH1D **h_peak_sum_per_fragment; 
  TH1D **h_peak_time_per_fragment; 
  TH1D **h_integrated_sum_per_fragment; 

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
  _last_event_number = 0;

}

void TriggerAna::beginJob()
{
  art::ServiceHandle<art::TFileService> tfs;
  h_ssp_adc = tfs->make<TH1D>("ssp_adc_values","ssp_adc_values",SHRT_MAX,-0.5,SHRT_MAX-0.5);  
  h_delta_event_number = tfs->make<TH1D>("delta_event_number","Difference between this and next event number",100,-0.5,100-0.5);  
  h_number_of_triggers = tfs->make<TH1D>("number_of_triggers", "Total number of triggers in an event",100,-0.5,100-0.5);

  std::string hist_name;
  std::string hist_title;

  hist_name = "trigger_type";
  hist_title = "Trigger type";  
  h_trigger_type = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  hist_name = "trigger_id";
  hist_title = "Trigger id";    
  h_trigger_id = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  hist_name = "channel_id";
  hist_title = "Channel id";    
  h_channel_id = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  hist_name = "peak_sum";
  hist_title = "Peak sum";    
  h_peak_sum = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  hist_name = "peak_time";
  hist_title = "Peak time";    
  h_peak_time = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  hist_name = "integrated_sum";
  hist_title = "Integrated sum";    
  h_integrated_sum = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

  _number_of_ssps = 6;

  if(_make_per_fragment_histos){
    h_number_of_triggers_per_fragment = new TH1D*[_number_of_ssps];

    h_trigger_type_per_fragment = new TH1D*[_number_of_ssps];
    h_trigger_id_per_fragment = new TH1D*[_number_of_ssps];
    h_channel_id_per_fragment = new TH1D*[_number_of_ssps];
    h_peak_sum_per_fragment = new TH1D*[_number_of_ssps];
    h_peak_time_per_fragment = new TH1D*[_number_of_ssps];
    h_integrated_sum_per_fragment = new TH1D*[_number_of_ssps];

    for(size_t ssp=0;ssp<_number_of_ssps;ssp++){
      hist_name = "number_of_triggers_fragment_" + std::to_string(ssp);
      hist_title = "Total number of triggers in fragment " + std::to_string(ssp);
      h_number_of_triggers_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(), hist_title.c_str(),100,-0.5,100-0.5);

      hist_name = "trigger_type_fragment_" + std::to_string(ssp);
      hist_title = "Trigger type in fragment " + std::to_string(ssp);
      h_trigger_type_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5);

      hist_name = "trigger_id_fragment_" + std::to_string(ssp);
      hist_title = "Trigger id in fragment " + std::to_string(ssp);
      h_trigger_id_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5); 

      hist_name = "channel_id_fragment_" + std::to_string(ssp);
      hist_title = "Channel id in fragment " + std::to_string(ssp);
  
      hist_name = "peak_sum_fragment_" + std::to_string(ssp);
      hist_title = "Peak sum in fragment " + std::to_string(ssp);
      h_peak_sum_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5); 

      hist_name = "peak_time_fragment_" + std::to_string(ssp);
      hist_title = "Peak time in fragment " + std::to_string(ssp);
      h_peak_time_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5); 

      hist_name = "integrated_sum_fragment_" + std::to_string(ssp);
      hist_title = "Integrated sum in fragment " + std::to_string(ssp);
      h_integrated_sum_per_fragment[ssp] = tfs->make<TH1D>(hist_name.c_str(),hist_title.c_str(), SHRT_MAX,-0.5,SHRT_MAX-0.5); 

    }//loop over ssp number
  }//if _make_per_fragment_histos
}


void TriggerAna::analyze(art::Event const & evt)
{
  // Implementation of required member function here.
  _number_of_triggers=0;

  _nEvents++;
  bool trigger_decision = false;
  auto eventID = evt.id();
  art::EventNumber_t   _this_event_number = evt.event();

  h_delta_event_number->Fill(_this_event_number-_last_event_number);

  _last_event_number=_this_event_number;

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
      _number_of_trggers_per_fragment=0;
      const auto& fragment((*ssp_fragments)[idx]);      
      dune::SSPFragment ssp_fragment(fragment);
      
      if (_verbose){
        std::cout << "        SSP fragment "  << fragment.fragmentID() 
                  << " has total size: "      << ssp_fragment.hdr_event_size()
                  << " and run number: "      << ssp_fragment.hdr_run_number()
                  << " with "                 << ssp_fragment.total_adc_values() 
                  << " total ADC values"      << std::endl;
      }

      const SSPDAQ::MillisliceHeader* ssp_header=0;

      if(fragment.hasMetadata()) {
        ssp_header = &(fragment.metadata<dune::SSPFragment::Metadata>()->sliceHeader);
        
        _number_of_triggers+=ssp_header->nTriggers;
        _number_of_trggers_per_fragment+=ssp_header->nTriggers;

        if(idx < _number_of_ssps && _make_per_fragment_histos) h_number_of_triggers_per_fragment[idx]->Fill(_number_of_trggers_per_fragment);

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
        


        //Fill per trigger information 
        h_trigger_type->Fill((daqHeader->group1 & 0xFF00) >> 8);
        h_trigger_id->Fill(daqHeader->triggerID);
        h_channel_id->Fill((daqHeader->group2 & 0x000F) >> 0);
        h_peak_sum->Fill(peaksum);
        h_peak_time->Fill((daqHeader->group3 & 0xFF00) >> 8);
        h_integrated_sum->Fill(((unsigned int)(daqHeader->intSumHigh) << 8) + (((unsigned int)(daqHeader->group4) & 0xFF00) >> 8));


        if(idx < _number_of_ssps && _make_per_fragment_histos)
        {
          h_trigger_type_per_fragment[idx]->Fill((daqHeader->group1 & 0xFF00) >> 8);
          h_trigger_id_per_fragment[idx]->Fill(daqHeader->triggerID);
          h_channel_id_per_fragment[idx]->Fill((daqHeader->group2 & 0x000F) >> 0);
          h_peak_sum_per_fragment[idx]->Fill(peaksum);
          h_peak_time_per_fragment[idx]->Fill((daqHeader->group3 & 0xFF00) >> 8);
          h_integrated_sum_per_fragment[idx]->Fill(((unsigned int)(daqHeader->intSumHigh) << 8) + (((unsigned int)(daqHeader->group4) & 0xFF00) >> 8));
        }

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

  h_number_of_triggers->Fill(_number_of_triggers);

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
