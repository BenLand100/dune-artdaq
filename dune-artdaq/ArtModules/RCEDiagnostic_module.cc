////////////////////////////////////////////////////////////////////////
// Class:       RCEDiagnostic
// Module Type: analyzer
// File:        RCEDiagnostic_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "canvas/Utilities/Exception.h"
#include "dune-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/TpcMicroSlice.hh"
#include "artdaq-core/Data/Fragments.hh"

#include "TH1.h"
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
class RCEDiagnostic;
}

class dune::RCEDiagnostic : public art::EDAnalyzer {

public:
  explicit RCEDiagnostic(fhicl::ParameterSet const & pset);
  virtual ~RCEDiagnostic();
  
  virtual void analyze(art::Event const & evt);
  
private:

  void beginJob();
  void endJob();
  
  static const int nRCE=16;

  
  std::string raw_data_label_;
  std::string frag_type_;
  TGraph *gError27[nRCE];
  TGraph *gError28[nRCE];
  TGraph *gError31[nRCE];
  TGraph *gErrorEvent;
  TGraph *gFWTimeStampError[nRCE];
  TGraph *gFWFirstTimeStampError[nRCE];
  TGraph *gFWMisAlign[nRCE][4];
  TGraph *gFWNOvAError[nRCE][4];
  TGraph *gFWTagIDError[nRCE][4];
  TGraph *gFWLaneOutOfSync[nRCE][4];
  TGraph *gTriggerTime[nRCE];
  TGraph *gNanoTime[nRCE];
  TGraph *gNanoTimeRelToTrig[nRCE];
  TGraph *gTriggerTimeRelToRCE0[nRCE];
  TGraph *gNanoTimeRelToRCE0[nRCE];
  TGraph *gADCVsTimeCh0[nRCE];

  //counters...
  int error27[nRCE];
  int error28[nRCE];
  int error31[nRCE];
  int errorts[nRCE];
  int errorfts[nRCE];
  int errormisalign[nRCE][4];
  int errorNOvA[nRCE][4];
  int errorTagIDError[nRCE][4];
  int errorLaneSync[nRCE][4];
  int n_millislices_to_chunk=10;
  int n_millislices[nRCE];  
  int n_chunks[nRCE];

  uint trigTime[nRCE];
  uint nanoTime[nRCE];
  uint16_t adc0[nRCE];
  int n_triggers;
  int n_micro_with_trigger[nRCE];
  int nPtsPerRCE[nRCE];
  int nPtsPerDiff[nRCE];
  bool event_has_trigger;


};

dune::RCEDiagnostic::RCEDiagnostic(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type"))
{
 
  n_triggers=0;
 gErrorEvent=nullptr;
  for(int j=0;j<nRCE;j++){
    gError27[j]=nullptr;
    gError28[j]=nullptr;
    gError31[j]=nullptr;
    gFWTimeStampError[j]=nullptr;
    gFWFirstTimeStampError[j]=nullptr;    
    gTriggerTime[j]=nullptr;
    gNanoTime[j]=nullptr;
    gTriggerTimeRelToRCE0[j]=nullptr;
    gNanoTimeRelToRCE0[j]=nullptr;
    gNanoTimeRelToTrig[j]=nullptr;
    gADCVsTimeCh0[j]=nullptr;
    nPtsPerRCE[j]=0;
    nPtsPerDiff[j]=0;
    n_millislices[j]=0;
    n_chunks[j]=0;
   for(int i = 0 ;i<4;i++){
      gFWMisAlign[j][i]=nullptr;
      gFWNOvAError[j][i]=nullptr;
      gFWTagIDError[j][i]=nullptr;
      gFWLaneOutOfSync[j][i]=nullptr;
      errormisalign[j][i]=0;
      errorNOvA[j][i]=0;
      errorTagIDError[j][i]=0;
      errorLaneSync[j][i]=0;
    }
  }
}

void dune::RCEDiagnostic::beginJob(){
  cout<<"beginJob::setting up histograms"<<endl;
  art::ServiceHandle<art::TFileService> tfs;
    gErrorEvent = tfs->make<TGraph>();
    gErrorEvent->SetName("ErrorPerEvent");
    gErrorEvent->SetTitle("Total Number of Errors per Event");
  for(int j=0;j<nRCE;j++){
    TString dir="RCE";
    dir+=j;dir+="/";
    gError27[j] = tfs->make<TGraph>();
    gError27[j]->SetName(dir+"Error27PerBoard");
    gError27[j]->SetTitle("Number of Errors 27(Missing NOvA Time Gap) per 128-channel Block");
    gError28[j] = tfs->make<TGraph>();
    gError28[j]->SetName(dir+"Error28PerBoard");
    gError28[j]->SetTitle("Number of Errors 28(Droped Frame Payload) per 128-channel Block");
    gError31[j] = tfs->make<TGraph>();
    gError31[j]->SetName(dir+"Error31PerBoard");
    gError31[j]->SetTitle("Number of Errors 31(Soft./Firmware Error) per 128-channel Block");
    
    gFWTimeStampError[j]= tfs->make<TGraph>();
    gFWTimeStampError[j]->SetName(dir+"TimeStampErrorPerBoard");
    gFWFirstTimeStampError[j]= tfs->make<TGraph>();
    gFWFirstTimeStampError[j]->SetName(dir+"FirstTimeStampErrorPerBoard");

    gTriggerTime[j] = tfs->make<TGraph>();
    gTriggerTime[j]->SetName(dir+"TriggerTime");
    gTriggerTime[j]->SetTitle("Trigger Time vs Trigger number for rce"+TString(j));

    gNanoTime[j] = tfs->make<TGraph>();
    gNanoTime[j]->SetName(dir+"NanoTime");
    gNanoTime[j]->SetTitle("First Nanoslice Time vs Trigger number for rce"+TString(j));

    gTriggerTimeRelToRCE0[j] = tfs->make<TGraph>();
    gTriggerTimeRelToRCE0[j]->SetName(dir+"TriggerTimeRelToRCE0");
    gTriggerTimeRelToRCE0[j]->SetTitle("Trigger Time Relative to RCE0 vs Trigger number for rce"+TString(j));

    gNanoTimeRelToRCE0[j] = tfs->make<TGraph>();
    gNanoTimeRelToRCE0[j]->SetName(dir+"NanoTimeRelToRCE0");
    gNanoTimeRelToRCE0[j]->SetTitle("Nano Time Relative to RCE0 vs Trigger number for rce"+TString(j));

    gNanoTimeRelToTrig[j] = tfs->make<TGraph>();
    gNanoTimeRelToTrig[j]->SetName(dir+"NanoTimeRelToTrig");
    gNanoTimeRelToTrig[j]->SetTitle("Nano Time Relative to Trigger vs Trigger number for rce"+TString(j));

    gADCVsTimeCh0[j]= tfs->make<TGraph>();
    gADCVsTimeCh0[j]->SetName(dir+"AdcVsTimeCh0");
    gADCVsTimeCh0[j]->SetTitle("Adc Value vs Time for Channel 0"+TString(j));

    for(int i = 0 ;i<4;i++){
      TString lane="Lane";
      lane+=i;
      gFWMisAlign[j][i]=tfs->make<TGraph>();
      gFWMisAlign[j][i]->SetName(dir+"MisAlignmentErrorsPerBoard"+lane);
      gFWNOvAError[j][i]=tfs->make<TGraph>();
      gFWNOvAError[j][i]->SetName(dir+"NOvAErrorsPerBoard"+lane);
      gFWTagIDError[j][i]=tfs->make<TGraph>();
      gFWTagIDError[j][i]->SetName(dir+"TagIdErrorsPerBoard"+lane);
      gFWLaneOutOfSync[j][i]=tfs->make<TGraph>();
      gFWLaneOutOfSync[j][i]->SetName(dir+"LaneOutOfSyncErrorsPerBoard"+lane);
    } 
  }
  cout<<"beginJob::done"<<endl;

}

void dune::RCEDiagnostic::endJob(){
  gErrorEvent->Write();
  for(int j=0;j<nRCE;j++){
    if(gError31[j]->GetN()>0){
      gError27[j]->Write();
      gError28[j]->Write();
      gError31[j]->Write();
      gFWTimeStampError[j]->Write();
      gFWFirstTimeStampError[j]->Write(); 
      gTriggerTime[j]->Write();
      gNanoTime[j]->Write();
      gTriggerTimeRelToRCE0[j]->Write();
      gNanoTimeRelToRCE0[j]->Write();
      gNanoTimeRelToTrig[j]->Write();
      gADCVsTimeCh0[j]->Write();
      for(int i = 0 ;i<4;i++){
	gFWMisAlign[j][i]->Write();
	gFWNOvAError[j][i]->Write();
	gFWTagIDError[j][i]->Write();
	gFWLaneOutOfSync[j][i]->Write();
      }
    }
  }
}

dune::RCEDiagnostic::~RCEDiagnostic(){
}

void dune::RCEDiagnostic::analyze(art::Event const & evt){
 
  art::EventNumber_t eventNumber = evt.event();
  cout<<"analyze::event number "<<eventNumber<<endl;

  int errorevent = 0;
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
    for(int n=0;n<nRCE;n++){
      trigTime[n]=0;
      nanoTime[n]=0;
      adc0[n]=0;
      n_micro_with_trigger[n]=0;
    }
    event_has_trigger=false;
    for (size_t idx = 0; idx < raw->size(); ++idx){
      const auto& frag((*raw)[idx]);
    
      int rceID=frag.fragmentID()-100;
      ///> Create a TpcMilliSliceFragment from the generic artdaq fragment
      TpcMilliSliceFragment msf(frag);	
      ///> get the total size of the millislice (data+header, in bytes)
      //dune::TpcMilliSlice::Header::millislice_size_t msf_size = msf.size();
      ///> get the number of MicroSlices in the MilliSlice
      dune::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();     
      //      cout<<"Microslice count "<<msf_us_count<<endl;
      if(n_millislices[rceID]>n_millislices_to_chunk){
	std::cout<<"Filling histograms with errors : "
		 <<error27<<"  "<<error28<<"  "<<error31
		 <<"  "<<errorts<<"   "<<errorfts<<std::endl;
	gError27[rceID]->SetPoint(n_chunks[rceID], n_chunks[rceID], error27[rceID]);
	gError28[rceID]->SetPoint(n_chunks[rceID], n_chunks[rceID], error28[rceID]);
	gError31[rceID]->SetPoint(n_chunks[rceID], n_chunks[rceID], error31[rceID]);
	gFWTimeStampError[rceID]->SetPoint(n_chunks[rceID],n_chunks[rceID], errorts[rceID]);	
	gFWFirstTimeStampError[rceID]->SetPoint(n_chunks[rceID],n_chunks[rceID],errorfts[rceID]);	
	//reset things
	error27[rceID] = 0;
	error28[rceID] = 0;
	error31[rceID] = 0;
	errorts[rceID]=0;
	errorfts[rceID]=0;
	n_millislices[rceID]=0;
	for(int i=0;i<4;i++){
	  //   std::cout<<"Lane "
	  //		       <<" MisAlign = "<<errormisalign[i]
	  //	       <<"; NOvA TS = "<<errorNOvA[i]
	  //	       <<"; Lane Sync = "<<errorLaneSync[i]
	  //	       <<"; Tag ID = "<<errorTagIDError[i]<<std::endl;
	  
	  gFWMisAlign[rceID][i]->SetPoint(n_chunks[rceID], n_chunks[rceID],errormisalign[rceID][i]);
	  gFWNOvAError[rceID][i]->SetPoint(n_chunks[rceID],n_chunks[rceID],errorNOvA[rceID][i]);
	  gFWTagIDError[rceID][i]->SetPoint(n_chunks[rceID], n_chunks[rceID],errorTagIDError[rceID][i]);
	  gFWLaneOutOfSync[rceID][i]->SetPoint(n_chunks[rceID], n_chunks[rceID],errorLaneSync[rceID][i]);
	  errormisalign[rceID][i]=0;
	  errorNOvA[rceID][i]=0;
	  errorTagIDError[rceID][i]=0;
	  errorLaneSync[rceID][i]=0;
	}
	n_chunks[rceID]++;
	cout<<"done filling"<<endl;
      }
      
      
      ///> loop over the number of microslices
      for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us){
	///> get the 'i_us-th' microslice
	std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);
	
	if (!microslice)
	  throw cet::exception("Error in RCEDiagnostic module: unable to find requested microslice");
	
	///> get the microslice type ID
	//dune::TpcMicroSlice::Header::type_id_t us_type_id = microslice->type_id();
	//	
	//	if ( std::bitset<4>((us_type_id >> 27 ) & 0xF) == 1){
	if (microslice->timeGap()){
	  errorevent++;
	  error27[rceID]++;
	}
	//	if ( std::bitset<4>((us_type_id >> 28 ) & 0xF) == 1)
	if ( microslice->droppedFrame())
	  error28[rceID]++;
	//	if ( std::bitset<4>((us_type_id >> 31 ) & 0xF) == 1){
	if( microslice->errorFlag()){
	  //	  std::cout<<" software/firmware error "<<std::endl;
	  errorevent++;
	  error31[rceID]++;
	}
	
	//uint has_trig = uint( ((us_type_id >> 29) & 0x1));  //this _should_ be the external trigger bit, but for some reason looks like it's not set correctly anymore...use the microslice size instead
	//	std::cout<<"Trigger? "<<has_trig<<"   "<<us_type_id<<"   "<<(us_type_id>>29)<<std::endl;
	
	uint has_trig=0;
	if (microslice->size()>28){ 
	  has_trig=1;
	  //	  std::cout<<"Trigger? "<<has_trig<<"   "<<microslice->size()<<endl;
	}
	///> get the software message
	dune::TpcMicroSlice::Header::softmsg_t us_software_message = microslice->software_message();
	//gTimeEvent->SetPoint(channelblock, channelblock, int( ((us_software_message >> 56) & 0xFF) ));
	if(has_trig){
	  event_has_trigger=true;
	  if(n_micro_with_trigger[rceID]==0){
	    trigTime[rceID]=uint( (us_software_message)& 0xFFFFFFFF );//take first 32-bits
	    nanoTime[rceID]=uint( (microslice->nanosliceNova_timestamp(0)) & 0xFFFFFFFF);
	    if(!microslice->nanosliceSampleValue(0,0,adc0[rceID]) )
	      std::cout<<"can't get adc value for this nanoslice!"<<std::endl;
	  }
	  n_micro_with_trigger[rceID]++;
	  //	  std::cout<<"Trigger time = "<<trigTime[rceID]<<std::endl;
	} else {
	  uint non_trig_time=uint( (us_software_message)& 0xFFFFFFFF );//take first 32-bits
	  std::cout<<"RCE"<<rceID<<"  Non-triggered time = "<<non_trig_time<<std::endl;
	}
		
	///> get the firmware message 
	dune::TpcMicroSlice::Header::firmmsg_t us_firmware_message = microslice->firmware_message();
	
	//	std::cout<<" firmware message = "<<us_firmware_message<<std::endl;
	  
	if(((us_firmware_message>> 62) & 0x1)==1)
	  errorts[rceID]++;
	if(((us_firmware_message>> 61) & 0x1)==1)
	  errorfts[rceID]++;
	
	for(int i=0;i<4;i++){
	  if(((us_firmware_message>>(52+i))& 0x1)==1)
	    errormisalign[rceID][i]++;
	  if(((us_firmware_message>>(48+i))& 0x1)==1)
	    errorNOvA[rceID][i]++;
	  if(((us_firmware_message>>(44+i))& 0x1)==1)
	    errorTagIDError[rceID][i]++;
	  if(((us_firmware_message>>(40+i))& 0x1)==1)
	    errorLaneSync[rceID][i]++;
	}	
      }// i_us                                                                                 
      n_millislices[rceID]++;    
    } //idx
    //    std::cout<<"Errors in the event "<<errorevent<<std::endl;     
    gErrorEvent->SetPoint(eventNumber, eventNumber, errorevent);

    if(event_has_trigger){
      for(int n=0;n<nRCE;n++){
	if(n_micro_with_trigger[n]>0){//make sure we some microslices with ADCs in this RCE
	  gTriggerTime[n]->SetPoint(nPtsPerRCE[n],n_triggers,trigTime[n]);
	  gNanoTime[n]->SetPoint(nPtsPerRCE[n],n_triggers,nanoTime[n]);
	  gNanoTimeRelToTrig[n]->SetPoint(nPtsPerRCE[n],n_triggers,int(nanoTime[n]-trigTime[n]));
	  gADCVsTimeCh0[n]->SetPoint(nPtsPerRCE[n],n_triggers,adc0[n]);	    
	  cout<<"ADC0 Value = "<<adc0[n]<<endl;
	  nPtsPerRCE[n]++;
	  std::cout<<"RCE = "<<n<<"::trigger time = "<<trigTime[n]
		   <<"; nanoTime = "<<nanoTime[n]<<std::endl;
	  if(n_micro_with_trigger[0]==n_micro_with_trigger[n]){//make sure we're looking at the same microslice
	    int trig_diff=trigTime[n]-trigTime[0];
	    int nano_diff=nanoTime[n]-nanoTime[0];
	    gTriggerTimeRelToRCE0[n]->SetPoint(nPtsPerDiff[n],n_triggers,trig_diff);
	    gNanoTimeRelToRCE0[n]->SetPoint(nPtsPerDiff[n],n_triggers,nano_diff);	    
	    nPtsPerDiff[n]++;
	  }
	  for (int m=n+1;m<nRCE;m++){
	    if(n_micro_with_trigger[n]!=n_micro_with_trigger[m]&&n_micro_with_trigger[m]>0 && n_micro_with_trigger[n]>0){
	      cout<<"number of microslices with payload in RCEs "<<n<<" and "<<m<<" don't match"<<endl;
	      cout<<"/t/t trigger times: RCE"<<n<<"="<<trigTime[n]
		  <<"; RCE"<<m<<"="<<trigTime[m]<<endl;
	      
	      cout<<"/t/t nanoslice times: RCE"<<n<<"="<<nanoTime[n]
		  <<"; RCE"<<m<<"="<<nanoTime[m]<<endl;
	      
	    }
	  }
	}
      }
      n_triggers++;
    }

  }//raw.IsValid()?
  else{
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has zero"
	      << " TpcMilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;

}

DEFINE_ART_MODULE(dune::RCEDiagnostic)
