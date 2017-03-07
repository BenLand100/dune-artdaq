////////////////////////////////////////////////////////////////////////
// Class:       SSPDump
// Module Type: analyzer
// File:        SSPDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "canvas/Utilities/Exception.h"
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "dune-raw-data/Overlays/anlTypes.hh"
#include "artdaq-core/Data/Fragments.hh"

#include "TH1.h"
#include "TFile.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>

namespace dune {
  class SSPDump;
}

using namespace dune;

class dune::SSPDump : public art::EDAnalyzer {
public:
  explicit SSPDump(fhicl::ParameterSet const & pset);
  virtual ~SSPDump();

  virtual void analyze(art::Event const & evt);

private:
  void beginJob() override;
  void endJob  () override;
  void beginEvent(art::EventNumber_t eventNumber);
  void endEvent  (art::EventNumber_t eventNumber);

  std::string raw_data_label_;
  std::string frag_type_;

  //sets the verbosity of std::cout printing
  //std::vector<int> verb_microslice_ids_;
  //std::vector<int> verb_nanoslice_ids_;
  uint32_t         verb_adcs_;
  bool             verb_meta_;

  //histograms, counters, etc
  TH1D * adc_values_;
  TH1D * all_adc_values_;
  TH1D * n_event_triggers_;
  uint32_t n_adc_counter_;  //counter of total number of ALL adc values in an event
  uint64_t adc_cumulative_; //cumulative total of ALL adc values in an event
};


dune::SSPDump::SSPDump(fhicl::ParameterSet const & pset)
    : EDAnalyzer(pset),
      raw_data_label_(pset.get<std::string>("raw_data_label")),
      frag_type_(pset.get<std::string>("frag_type")),
      //verb_microslice_ids_(pset.get<std::vector<int>>("verbose_microslice_ids", std::vector<int>(1,0))),
      //verb_nanoslice_ids_ (pset.get<std::vector<int>>("verbose_nanoslice_ids",  std::vector<int>(1,0))),
      verb_adcs_(pset.get<uint32_t>        ("verbose_adcs", 10000)),
      verb_meta_(pset.get<bool>            ("verbose_metadata", true)),
      adc_values_(nullptr),
      all_adc_values_(nullptr),
      n_event_triggers_(nullptr),
      n_adc_counter_(0),
      adc_cumulative_(0)
{
}

void dune::SSPDump::beginJob()
{
  art::ServiceHandle<art::TFileService> tfs;
  adc_values_ = tfs->make<TH1D>("adc_values","adc_values",4096,-0.5,4095.5);  
  all_adc_values_ = tfs->make<TH1D>("all_adc_values","all_adc_values",4096,-0.5,4095.5);  
  n_event_triggers_ = tfs->make<TH1D>("n_event_triggers","n_event_triggers",960,-0.5,959.5);  
}

void dune::SSPDump::beginEvent(art::EventNumber_t /*eventNumber*/)
{
  //reset ADC histogram
  adc_values_->Reset();
  //reset counters
  n_adc_counter_  = 0;
  adc_cumulative_ = 0;
}

void dune::SSPDump::endEvent(art::EventNumber_t eventNumber)
{
  //write the ADC histogram for the given event
  if(n_adc_counter_)
    adc_values_->Write(Form("adc_values:event_%d", eventNumber));
}

void dune::SSPDump::endJob()
{
  delete adc_values_;
}

dune::SSPDump::~SSPDump()
{
}

void dune::SSPDump::analyze(art::Event const & evt)
{
  art::EventNumber_t eventNumber = evt.event();
  beginEvent(eventNumber);
  
  //  TFile f("hists.root","RECREATE");
  // ***********************
  // *** SSP Fragments ***
  // ***********************
  
  ///> look for raw SSP data  
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);
  
  if (raw.isValid()) {
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has " << raw->size()
              << " fragment(s) of type " << frag_type_ << std::endl;
    
    unsigned int allTriggersProcessed = 0;

    std::map<int, int> triggers_per_fragment;

    for (size_t idx = 0; idx < raw->size(); ++idx) {
      const auto& frag((*raw)[idx]);
      
      ///> Create a SSPFragment from the generic artdaq fragment
      SSPFragment sspf(frag);
      
      ///> get the size of the event in units of dune::SSPFragment::Header::data_t
      dune::SSPFragment::Header::event_size_t event_size = sspf.hdr_event_size();

      ///> get the size of the header in units of dune::SSPFragment::Header::data_t
      std::size_t header_size = sspf.hdr_run_number();

      ///> get the event run number
      dune::SSPFragment::Header::run_number_t run_number = sspf.hdr_run_number();
      
      ///> get the number of ADC values describing data beyond the header
      std::size_t n_adc_values = sspf.total_adc_values();

      std::cout << std::endl;
      std::cout << "SSP fragment "     << frag.fragmentID() 
		<< " has total size: " << event_size << " SSPFragment::Header::data_t words"
		<< " (of which " << header_size << " is header)"
		<< " and run number: " << run_number
		<< " with " << n_adc_values << " total ADC values"
		<< std::endl;
      std::cout << std::endl;
      
      const SSPDAQ::MillisliceHeader* meta=0;
      ///> get the information from the header
      if(frag.hasMetadata())
	{
	  ///> get the metadata
	  meta = &(frag.metadata<SSPFragment::Metadata>()->sliceHeader);
	  
	  ///> get the start and end times for the millislice
	  unsigned long start_time = meta->startTime;
	  unsigned long end_time   = meta->endTime;

	  ///> get the length of the millislice in unsigned ints (including header)
	  unsigned int milli_length = meta->length;

	  ///> get the number of triggers in the millislice
	  unsigned int n_triggers = meta->nTriggers;

	  std::cout
	    <<"===Slice metadata:"<<std::endl
	    <<"Start time         "<< start_time   <<std::endl
	    <<"End time           "<< end_time     <<std::endl
	    <<"Packet length      "<< milli_length <<std::endl
	    <<"Number of triggers "<< n_triggers   <<std::endl <<std::endl;
	}
      else
	{
	  std::cout << "SSP fragment has no metadata associated with it." << std::endl;
	}
      
      ///> get a pointer to the first trigger in the millislice
      const unsigned int* dataPointer = sspf.dataBegin();
      
      ///> loop over the triggers in the millislice
      unsigned int triggersProcessed=0;
      while((meta==0||triggersProcessed<meta->nTriggers)&&dataPointer<sspf.dataEnd()){

	///> get the trigger header
	const SSPDAQ::EventHeader* daqHeader=reinterpret_cast<const SSPDAQ::EventHeader*>(dataPointer);
	
	///> get the 'start of header word' (should always be 0xAAAAAAAA)
	unsigned int trig_header = daqHeader->header;

	///> get the length of the trigger in unsigned ints (including header)
	unsigned short trig_length = daqHeader->length;

	///> get the trigger type
	unsigned short trig_trig_type = ((daqHeader->group1 & 0xFF00) >> 8);

	///> get the status flags
	unsigned short trig_status_flags = ((daqHeader->group1 & 0x00F0) >> 4);

	///> get the header type
	unsigned short trig_header_type = ((daqHeader->group1 & 0x000F) >> 0);

	///> get the trigger ID
	unsigned short trig_trig_id = daqHeader->triggerID;

	///> get the module ID
	unsigned short trig_module_id = ((daqHeader->group2 & 0xFFF0) >> 4);

	///> get the channel ID
	unsigned short trig_channel_id = ((daqHeader->group2 & 0x000F) >> 0);

	///> get the external timestamp sync delay (FP mode)
	unsigned int trig_timestamp_sync_delay = ((unsigned int)(daqHeader->timestamp[1]) << 16) + (unsigned int)(daqHeader->timestamp[0]);

	///> get the external timestamp sync count (FP mode)
	unsigned int trig_timestamp_sync_count = ((unsigned int)(daqHeader->timestamp[3]) << 16) + (unsigned int)(daqHeader->timestamp[2]);

	///> get the external timestamp (NOvA mode)
	unsigned long trig_timestamp_nova = ((unsigned long)daqHeader->timestamp[3] << 48) + ((unsigned long)daqHeader->timestamp[2] << 32)
	  + ((unsigned long)daqHeader->timestamp[1] << 16) + ((unsigned long)daqHeader->timestamp[0] << 0);
	
	///> get the peak sum
	uint32_t trig_peaksum = ((daqHeader->group3 & 0x00FF) >> 16) + daqHeader->peakSumLow;
	if(trig_peaksum & 0x00800000) {
	  trig_peaksum |= 0xFF000000;
	}

	///> get the peak time
	unsigned short trig_peaktime = ((daqHeader->group3 & 0xFF00) >> 8);

	///> get the prerise
	unsigned int trig_prerise = ((daqHeader->group4 & 0x00FF) << 16) + daqHeader->preriseLow;

	///> get the integrated sum
	unsigned int trig_intsum = ((unsigned int)(daqHeader->intSumHigh) << 8) + (((unsigned int)(daqHeader->group4) & 0xFF00) >> 8);

	///> get the baseline
	unsigned short trig_baseline = daqHeader->baseline;

	///> get the CFD timestamp interpolation points
	unsigned short trig_cfd_interpol[4];
	for(unsigned int i_cfdi = 0; i_cfdi < 4; i_cfdi++)
	  trig_cfd_interpol[i_cfdi] = daqHeader->cfdPoint[i_cfdi];

	///> get the internal interpolation point
	unsigned short trig_internal_interpol = daqHeader->intTimestamp[0];

	///> get the internal timestamp
	uint64_t trig_internal_timestamp = ((uint64_t)((uint64_t)daqHeader->intTimestamp[3] << 32)) + ((uint64_t)((uint64_t)daqHeader->intTimestamp[2]) << 16) + ((uint64_t)((uint64_t)daqHeader->intTimestamp[1]));

	if(verb_meta_) {
	  std::cout
	    << "Header:                             " << trig_header               << std::endl
	    << "Length:                             " << trig_length               << std::endl
	    << "Trigger type:                       " << trig_trig_type            << std::endl
	    << "Status flags:                       " << trig_status_flags         << std::endl
	    << "Header type:                        " << trig_header_type          << std::endl
	    << "Trigger ID:                         " << trig_trig_id              << std::endl
	    << "Module ID:                          " << trig_module_id            << std::endl
	    << "Channel ID:                         " << trig_channel_id           << std::endl
	    << "External timestamp (FP mode):       " << std::endl
	    << "  Sync delay:                       " << trig_timestamp_sync_delay << std::endl
	    << "  Sync count:                       " << trig_timestamp_sync_count << std::endl
	    << "External timestamp (NOvA mode):     " << trig_timestamp_nova       << std::endl
	    << "Peak sum:                           " << trig_peaksum              << std::endl
	    << "Peak time:                          " << trig_peaktime             << std::endl
	    << "Prerise:                            " << trig_prerise              << std::endl
	    << "Integrated sum:                     " << trig_intsum               << std::endl
	    << "Baseline:                           " << trig_baseline             << std::endl
	    << "CFD Timestamp interpolation points: " << trig_cfd_interpol[0]      << " " << trig_cfd_interpol[1] << " " << trig_cfd_interpol[2]   << " " << trig_cfd_interpol[3] << std::endl
	    << "Internal interpolation point:       " << trig_internal_interpol    << std::endl
	    << "Internal timestamp:                 " << trig_internal_timestamp   << std::endl
	    << std::endl;
	}

	///> increment the data pointer past the trigger header
	dataPointer+=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
	
	//>get the information from the data
	bool verb_values = true;

	///> get the number of ADC values in the trigger
	unsigned int nADC=(trig_length-sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int))*2;

	///> get a pointer to the first ADC value
	const unsigned short* adcPointer=reinterpret_cast<const unsigned short*>(dataPointer);

	char histName[100];
	sprintf(histName,"event%d",triggersProcessed);
	//TH1F* hist=new TH1F(histName,histName,nADC,0,nADC*1./150.);
	//	hist->SetDirectory(&f);

	///> increment over the ADC values
	for(size_t idata = 0; idata < nADC; idata++) {
	  ///> get the 'idata-th' ADC value
	  const unsigned short* adc = adcPointer + idata;

	  if(idata >= verb_adcs_)
	    verb_values = false;
	  else if(idata == 0&&verb_adcs_>0)
	    std::cout << "Printing the " << nADC << " ADC values saved with the trigger:" << std::endl;
	  
	  adc_values_->Fill(*adc);
	  all_adc_values_->Fill(*adc);
	  n_adc_counter_++;
	  adc_cumulative_ += (uint64_t)(*adc);
	  //	  hist->SetBinContent(idata+1,*adc);

	  if(verb_values)
	    std::cout << *adc << " ";
	}//idata

	///> increment the data pointer to the end of the current trigger (to the start of the next trigger header, if available)
	dataPointer+=nADC/2;

	++triggersProcessed;
	std::cout<<std::endl<<"Triggers processed: "<<triggersProcessed<<std::endl<<std::endl;
      }//triggers
      triggers_per_fragment[frag.fragmentID()] = triggersProcessed;
      allTriggersProcessed += triggersProcessed;
    }//fragments
    n_event_triggers_->Fill(allTriggersProcessed);
    std::cout << "Event " << eventNumber << " has " << allTriggersProcessed << " total triggers";
    for(std::map<int, int>::iterator i_triggers_per_fragment = triggers_per_fragment.begin(); i_triggers_per_fragment != triggers_per_fragment.end(); i_triggers_per_fragment++)
      std::cout << " " << i_triggers_per_fragment->first << ":" << i_triggers_per_fragment->second;
    std::cout << std::endl;
    std::cout << std::endl
	      << "Event ADC average is (from counter):   " << (double)adc_cumulative_/(double)n_adc_counter_
	      << std::endl
	      << "Event ADC average is (from histogram): " << adc_values_->GetMean()
	      << std::endl;
  }//raw.IsValid()?
  else {
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " SSP fragments." << std::endl;
  }
  std::cout << std::endl;
  //  f.Write();
  endEvent(eventNumber);
}

DEFINE_ART_MODULE(dune::SSPDump)
