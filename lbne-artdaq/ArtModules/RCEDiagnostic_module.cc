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

#include "art/Utilities/Exception.h"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
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

namespace lbne {
class RCEDiagnostic;
}

class lbne::RCEDiagnostic : public art::EDAnalyzer {

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
  TGraph *gTimeEvent[nRCE];

  TGraph *gFWTimeStampError[nRCE];
  TGraph *gFWFirstTimeStampError[nRCE];
  TGraph *gFWMisAlign[nRCE][4];
  TGraph *gFWNOvAError[nRCE][4];
  TGraph *gFWTagIDError[nRCE][4];
  TGraph *gFWLaneOutOfSync[nRCE][4];
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
};

lbne::RCEDiagnostic::RCEDiagnostic(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type"))
{
    gErrorEvent=nullptr;
  for(int j=0;j<nRCE;j++){
    gError27[j]=nullptr;
    gError28[j]=nullptr;
    gError31[j]=nullptr;
    gTimeEvent[j]=nullptr;
    gFWTimeStampError[j]=nullptr;
    gFWFirstTimeStampError[j]=nullptr;    
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

void lbne::RCEDiagnostic::beginJob(){
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
    
    gTimeEvent[j] = tfs->make<TGraph>();
    gTimeEvent[j]->SetName(dir+"TimeEvent");
    gTimeEvent[j]->SetTitle("Trigger Times x Event Number per 128-Channel Block");
    
    gFWTimeStampError[j]= tfs->make<TGraph>();
    gFWTimeStampError[j]->SetName(dir+"TimeStampErrorPerBoard");
    gFWFirstTimeStampError[j]= tfs->make<TGraph>();
    gFWFirstTimeStampError[j]->SetName(dir+"FirstTimeStampErrorPerBoard");
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
}

void lbne::RCEDiagnostic::endJob(){
  gErrorEvent->Write();
  for(int j=0;j<nRCE;j++){
    if(gError31[j]->GetN()>0){
      gTimeEvent[j]->Write();
      gError27[j]->Write();
      gError28[j]->Write();
      gError31[j]->Write();
      gFWTimeStampError[j]->Write();
      gFWFirstTimeStampError[j]->Write(); 
      for(int i = 0 ;i<4;i++){
	gFWMisAlign[j][i]->Write();
	gFWNOvAError[j][i]->Write();
	gFWTagIDError[j][i]->Write();
	gFWLaneOutOfSync[j][i]->Write();
      }
    }
  }
}

lbne::RCEDiagnostic::~RCEDiagnostic(){
}

void lbne::RCEDiagnostic::analyze(art::Event const & evt){
 
  art::EventNumber_t eventNumber = evt.event();

  int errorevent = 0;
  // ***********************
  // *** TpcMilliSlice Fragments ***
  // ***********************  
  ///> look for raw TpcMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);
  
  if (raw.isValid()) {
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has " << raw->size()
	      << " fragment(s) of type " << frag_type_ << std::endl;
    
    for (size_t idx = 0; idx < raw->size(); ++idx){
      const auto& frag((*raw)[idx]);
    
      int rceID=frag.fragmentID()-100;
      ///> Create a TpcMilliSliceFragment from the generic artdaq fragment
      TpcMilliSliceFragment msf(frag);	
      ///> get the total size of the millislice (data+header, in bytes)
      //lbne::TpcMilliSlice::Header::millislice_size_t msf_size = msf.size();
      ///> get the number of MicroSlices in the MilliSlice
      lbne::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();     
      
      if(n_millislices[rceID]>n_millislices_to_chunk){
	//	    std::cout<<"Filling histograms with errors : "
	//		     <<error27<<"  "<<error28<<"  "<<error31
	//		     <<"  "<<errorts<<"   "<<errorfts<<std::endl;
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
      }
      
      
      ///> loop over the number of microslices
      for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us){
	///> get the 'i_us-th' microslice
	std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);
	
	if (!microslice)
	  throw cet::exception("Error in RCEDiagnostic module: unable to find requested microslice");
	
	///> get the microslice type ID
	lbne::TpcMicroSlice::Header::type_id_t us_type_id = microslice->type_id();
	
	if ( std::bitset<4>((us_type_id >> 27 ) & 0xF) == 1){
	  errorevent++;
	  error27[rceID]++;
	}
	//if ( std::bitset<4>((us_type_id >> 28 ) & 0xF) == 1)
	//  error28++;
	if ( std::bitset<4>((us_type_id >> 31 ) & 0xF) == 1){
	  std::cout<<" software/firmware error = "<<(us_type_id>>31)<<std::endl;
	  errorevent++;
	  error31[rceID]++;
	}
	
	
	///> get the software message
	//lbne::TpcMicroSlice::Header::softmsg_t us_software_message = microslice->software_message();
	//gTimeEvent->SetPoint(channelblock, channelblock, int( ((us_software_message >> 56) & 0xFF) ));
	
	///> get the firmware message 
	lbne::TpcMicroSlice::Header::firmmsg_t us_firmware_message = microslice->firmware_message();
	
	std::cout<<" firmware message = "<<us_firmware_message<<std::endl;
	  
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
    std::cout<<"Errors in the event "<<errorevent<<std::endl;     
    gErrorEvent->SetPoint(eventNumber, eventNumber, errorevent);
  }//raw.IsValid()?
  else{
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has zero"
	      << " TpcMilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;

}

DEFINE_ART_MODULE(lbne::RCEDiagnostic)
