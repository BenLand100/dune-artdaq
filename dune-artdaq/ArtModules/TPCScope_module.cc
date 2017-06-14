////////////////////////////////////////////////////////////////////////
// Class:       TPCScope
// Module Type: analyzer
// File:        TPCScope_module.cc
// Description: Makes scope trace & FFT of contiguous blocks of ADC data
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "canvas/Utilities/Exception.h"
#include "dune-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/TpcMicroSlice.hh"
#include "artdaq-core/Data/Fragment.hh"

#include "TH1D.h"
#include "TGraph.h"
#include "TString.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>

using namespace std;

namespace dune {
class TPCScope;
}

class dune::TPCScope : public art::EDAnalyzer {

public:
  explicit TPCScope(fhicl::ParameterSet const & pset);
  virtual ~TPCScope();
  
  virtual void analyze(art::Event const & evt);
  
private:

  void beginJob();
  void endJob();
  void dumpPlots(uint rce,uint ch, uint cnt);
  static const int nRCE=16;
  static const int nChannels=128;
  typedef vector<uint16_t> uintvec;
  typedef vector<uint32_t> uint32vec;
  std::string raw_data_label_;
  std::string frag_type_;


  //counters...

  uint maxNumberOfTicksInScope ;
  uint holdOff;
  uint scope_channel_;
  uint timeIncrement=32; //32 64-MHz counts between 2-Mz ticks
  vector<uint32vec> traceCnt;

  //  uint scopeCnt[nRCE];
  //  uint previousScopeTime[nRCE];
  vector<uint32vec> scopeCnt;
  vector<uint32vec> previousScopeTime;

  vector< vector<uintvec> > scopeBuffers;

  vector<vector <TH1*> > hScopeTrace;
  //  vector<vector <TH1*> > hFFTMag;
  //  vector<vector <TH1*> > hFFTPhase;
  uint maxTraces;

  vector<int> channelsToLookAt;

};

dune::TPCScope::TPCScope(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type"))
{
  scope_channel_=pset.get<uint>("scope_channel",0);
  maxNumberOfTicksInScope=pset.get<uint>("max_number_of_ticks_in_scope",10000);//1s
  holdOff=pset.get<uint>("hold_off",10000);  
  maxTraces=pset.get<int>("max_traces",1);
  channelsToLookAt=pset.get<std::vector<int>>("channels_to_look_at",  std::vector<int>(1,0));
}

void dune::TPCScope::beginJob(){
  cout<<"beginJob::setting up histograms"<<endl;
  art::ServiceHandle<art::TFileService> tfs;
  //setup the arrays
  scopeBuffers.resize(nRCE);
  scopeCnt.resize(nRCE);
  traceCnt.resize(nRCE);
  previousScopeTime.resize(nRCE);
  hScopeTrace.resize(nRCE);
  //  hScopeFFTMag.resize(nRCE);
  // hScopeFFTPhase.resize(nRCE);
  for(int j=0;j<nRCE;j++){
    scopeBuffers[j].resize(nChannels);
    scopeCnt[j].resize(nChannels);
    traceCnt[j].resize(nChannels);
    previousScopeTime[j].resize(nChannels);
    hScopeTrace[j].resize(nChannels);
    //    hScopeFFTMag[j].resize(nChannels);
    //  hScopeFFTPhase[j].resize(nChannels);    
    for(int k=0;k<nChannels;k++){
      scopeCnt[j][k]=0;
      previousScopeTime[j][k]=0;      
      hScopeTrace[j][k]=nullptr;
      traceCnt[j][k]=0;
      //      hScopeFFTMag[j][k]=nullptr;
      //  hScopeFFTPhase[j][k]=nullptr;
    }
  }  
  cout<<"beginJob::done"<<endl;
}

void dune::TPCScope::endJob(){
  for(int j=0;j<nRCE;j++){
    //      cout<<"Doing FFT on RCE"<<j<<endl;
    //  hScopeMode[j]->FFT(hScopeFFT[j],"MAG");
    //  hScopeFFT[j]->Write();
  }
  
}

dune::TPCScope::~TPCScope(){
}

void dune::TPCScope::analyze(art::Event const & evt){
 
  art::EventNumber_t eventNumber = evt.event();
  cout<<"analyze::event number "<<eventNumber<<endl;
  // ***********************
  // *** TpcMilliSlice Fragments ***
  // ***********************  
  ///> look for raw TpcMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);
  cout<<"analyze::got event"<<endl;
  
  if (raw.isValid()) {
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has " << raw->size()
	      << " fragment(s) of type " << frag_type_ << std::endl;
    // reset some variables
    for (size_t idx = 0; idx < raw->size(); ++idx){
      const auto& frag((*raw)[idx]);
      
      int rceID=frag.fragmentID()-100;
      ///> Create a TpcMilliSliceFragment from the generic artdaq fragment
      TpcMilliSliceFragment msf(frag);	
      ///> get the total size of the millislice (data+header, in bytes)
      //dune::TpcMilliSlice::Header::millislice_size_t msf_size = msf.size();
      ///> get the number of MicroSlices in the MilliSlice
      dune::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();           
      
      ///> loop over the number of microslices
      for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us){
	///> get the 'i_us-th' microslice
	std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);
	
	if (!microslice)
	  throw cet::exception("Error in TPCScope module: unable to find requested microslice");
	
	//	  std::cout<<"Trigger? "<<has_trig<<"   "<<microslice->size()<<endl;
	//	uint8_t runmode=microslice->runMode();
	//loop over the nanoslices and fill ADC values
	for (uint i_ns=0;i_ns<microslice->nanoSliceCount();i_ns++){ //nanoslices in the microslice
	  std::unique_ptr<TpcNanoSlice> ns=microslice->nanoSlice(i_ns);
	  for (uint c=0;c<ns->getNChannels();c++){//channels in the nanoslice
	    bool useChannel = (std::find(channelsToLookAt.begin(), channelsToLookAt.end(), c) != channelsToLookAt.end()) ? true : false;
	    if(!useChannel || traceCnt[rceID][c]>maxTraces)//if we've already get enough or dont' want this channel, just continue
	      continue;
	    uint16_t adc=9999;
	    if(!ns->sampleValue(c,adc))
	      std::cout<<"can't get adc value for this nanoslice!"<<std::endl;
	    else{
	      if(scopeCnt[rceID][c]>=holdOff){
		//check the time if not in scope mode
		uint thisTime=uint(ns->nova_timestamp());
		//		cout<<"RCE = "<<rceID<<"; Channel = "<<c<<"  time = "<<thisTime<<"; scopeCnt ="<<scopeCnt[rceID][c]<<endl;
		if((scopeCnt[rceID][c]!=holdOff && thisTime!=previousScopeTime[rceID][c]+timeIncrement)|| scopeCnt[rceID][c]>=holdOff+maxNumberOfTicksInScope){		  
		  cout<<"Skipped a tick in scope! previous time (or reached max ticks)= "<<previousScopeTime[rceID][c]<<"; this time = "<<thisTime<<endl;
		  //done with continuous trace...plot every thing!
		  dumpPlots(rceID,c,traceCnt[rceID][c]++);
		}
		previousScopeTime[rceID][c]=thisTime;
		scopeBuffers[rceID][c].push_back(adc);		
		//		  gScopeMode[rceID]->SetPoint((scopeCnt[rceID]-holdOff),(scopeCnt[rceID]-holdOff),adc);
		// cout<<"Setting bin "<<(scopeCnt[rceID]-holdOff)<<" to "<<adc<<endl;
		// hScopeMode[rceID]->SetBinContent(uint(scopeCnt[rceID]-holdOff),adc);
		if(fabs(adc-750)>100){
		  //		  cout<<"Bad ADC???? tick number = "<<i_ns<<"; adc = " <<adc<<"; channel number = "<<c<<endl;
		}
	      }
	      //	      cout<<"RCE = "<<rceID<<"; Channel = "<<c<<"  ADC = "<<adc<<endl;
	      scopeCnt[rceID][c]++;
	    }	    
	}//channels
      }//nanoslices      	
    }//microslices       
    } //idx
  }//raw.IsValid()?
  
  else{
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has zero"
	      << " TpcMilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;
  
}

void dune::TPCScope::dumpPlots(uint rce, uint ch, uint cnt){
  art::ServiceHandle<art::TFileService> tfs;
  TString temp="RCE";temp+=rce;temp+="Channel";temp+=ch;temp+="Trace";temp+=cnt;

  uintvec trace=scopeBuffers[rce][ch];
  int nticks=trace.size();
  cout<<"Filling histograms for RCE= "<<rce<<"  Channel= "<<ch<<" with "<<nticks<<" ticks"<<endl;
  TH1D* scopeTrace=tfs->make<TH1D>(temp+"Scope",temp+"Scope",nticks,0,nticks);
  TH1D* scopeFFTMag=tfs->make<TH1D>(temp+"FFTMag",temp+"FFTMag",nticks/2,0,1000000);
  TH1D* scopeFFTPhase=tfs->make<TH1D>(temp+"FFTPhase",temp+"FFTPhase",nticks/2,-3.14,3.14);
  
  for(int i=0;i<nticks;i++){
    scopeTrace->SetBinContent(i,trace[i]);
  }
  
  cout<<"Doing magnitude FFT"<<endl;
  scopeTrace->FFT(scopeFFTMag,"MAG");
  cout<<"Doing phase FFT"<<endl;
  scopeTrace->FFT(scopeFFTPhase,"PH");

  scopeTrace->Write();
  scopeFFTMag->Write();
  scopeFFTPhase->Write();

  //reset the counters. 
  scopeBuffers[rce][ch].clear();
  scopeCnt[rce][ch]=0;
  
}


DEFINE_ART_MODULE(dune::TPCScope)
