////////////////////////////////////////////////////////////////////////
//
// art module to produce online monitoring plots from raw artdaq data
// April 2015, M Wallbank (m.wallbank@sheffield.ac.uk)
//
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "artdaq-core/Data/Fragments.hh"
//#include "tools/monitoringHistsStyle.C"

#include <vector>
#include <map>
#include <iostream>
#include <numeric>
#include <bitset>

#include <TH1.h>
#include <TH2.h>
#include <TProfile2D.h>
#include <TPad.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TCollection.h>
#include <TSystemFile.h>
#include <TObjArray.h>
#include <TList.h>
#include <TROOT.h>
#include <TPaveText.h>
#include <TPaveLabel.h>

namespace lbne {
  class OnlineMonitoring;
}

class lbne::OnlineMonitoring : public art::EDAnalyzer {

public:

  explicit OnlineMonitoring(fhicl::ParameterSet const &pset);
  virtual ~OnlineMonitoring();

  void analyze(art::Event const &event);
  void analyzeRCE(art::Handle<artdaq::Fragments> rawRCE);
  void analyzeSSP(art::Handle<artdaq::Fragments> rawSSP);
  void beginJob();
  void endJob();
  void eventDisplay();
  void monitoringGeneral();
  void monitoringRCE();
  void monitoringSSP();
  void reset();
  void windowingRCE();

private:

  art::EventNumber_t fEventNumber;
  unsigned int fRun;
  unsigned int fSubrun;

  std::map<unsigned int, unsigned int> tpcFragmentMap;
  std::map<unsigned int, unsigned int> sspFragmentMap;

  //const TString fDataDirName = "/lbne/data2/users/wallbank/";
  const TString fDataDirName = "/data/lbnedaq/data/";

  int fThreshold = 10;

  // Vectors to hold all information
  // channelvector<tickvector<ADC>>
  std::vector<std::vector<int> > fADC;
  std::vector<std::vector<int> > fWaveform;

  // Windowing
  std::vector<std::vector<short> > fWindowingBlockBegin;
  std::vector<std::vector<short> > fWindowingBlockSize;
  std::vector<int> fWindowingNumBlocks;
  const int fWindowingZeroThresholdSigned = 10;
  int fWindowingNearestNeighbour = 4;

  // Variables for monitoring
  int fTotalADC, fTotalWaveform;
  int fTotalRCEHitsEvent, fTotalSSPHitsEvent;
  int fTimesADCGoesOverThreshold, fTimesWaveformGoesOverThreshold;
  int fNumSubDetectorsPresent;

  // -----------------------------------------
  // Monitoring histograms

  // RCE
  TH2D *hAvADCChannelEvent;
  TProfile *hRCEDNoiseChannel;
  TH1I *hTotalADCEvent;
  TH1I *hTotalRCEHitsEvent;
  TH1I *hTotalRCEHitsChannel;
  TH1I *hNumMicroslicesInMillislice;
  TH1I *hTimesADCGoesOverThreshold;
  TProfile *hAsymmetry;
  std::map<int,TH2I*> hADCBits;
  std::map<int,TH2I*> hADCBitsAnd;
  std::map<int,TH2I*> hADCBitsOr;
  std::map<int,TH1I*> hADCChannel;

  // SSP
  TH2D *hAvWaveformChannelEvent;
  TProfile *hSSPDNoiseChannel;
  TH1I *hTotalWaveformEvent;
  TH1I *hTotalSSPHitsEvent;
  TH1I *hTotalSSPHitsChannel;
  TH1I *hTimesWaveformGoesOverThreshold;
  std::map<int,TH1I*> hWaveformChannel;

  // General
  TH1I *hNumSubDetectorsPresent;
  TH1I *hSizeOfFiles;
  TH1D *hSizeOfFilesPerEvent;

  // Canvases
  TCanvas *cAvADCChannelEvent;
  TCanvas *cAvWaveformChannelEvent;
  TCanvas *cRCEDNoiseChannel;
  TCanvas *cSSPDNoiseChannel;
  TCanvas *cTotalADCEvent;
  TCanvas *cTotalWaveformEvent;
  TCanvas *cTotalRCEHitsEvent;
  TCanvas *cTotalSSPHitsEvent;
  TCanvas *cTotalRCEHitsChannel;
  TCanvas *cTotalSSPHitsChannel;
  TCanvas *cTimesADCGoesOverThreshold;
  TCanvas *cTimesWaveformGoesOverThreshold;
  TCanvas *cADCBits;
  TCanvas *cADCBitsAnd;
  TCanvas *cADCBitsOr;
  TCanvas *cAsymmetry;
  TCanvas *cNumSubDetectorsPresent;
  TCanvas *cSizeOfFiles;
  TCanvas *cSizeOfFilesPerEvent;
  TPaveText *pTitle = new TPaveText(0.05,0.92,0.4,0.98,"brNDC");

  // -----------------------------------------

  bool _verbose = false;
  bool _daqtest = false;

  bool fIsRCE = true;
  bool fIsSSP = true;

  bool fIsInduction = true;

};

lbne::OnlineMonitoring::OnlineMonitoring(fhicl::ParameterSet const &pset) : EDAnalyzer(pset) {
}

lbne::OnlineMonitoring::~OnlineMonitoring() {
}

void lbne::OnlineMonitoring::beginJob() {
  gStyle->SetOptStat(0);
  //gStyle->SetOptTitle(1);
  //gROOT->ProcessLine(".x tools/monitoringHistsStyle.C");
  //monitoringHistsStyle();
  //gROOT->SetStyle(histStyle);

  // RCE hists
  hAvADCChannelEvent = new TH2D("hAvADCChannelEvent",";Event;Channel",100,0,100,2048,0,2048);
  hRCEDNoiseChannel = new TProfile("hRCEDNoiseChannel",";Channel;DNoise",2048,0,2048);
  hTotalADCEvent = new TH1I("hTotalADCEvent",";Total ADC;",100,0,100);
  hTotalRCEHitsEvent = new TH1I("hTotalRCEHits",";Total RCE Hits;",100,0,1000);
  hTotalRCEHitsChannel = new TH1I("hTotalRCEHitsChannel",";Channel;Total RCE Hits;",2048,0,2048);
  hNumMicroslicesInMillislice = new TH1I("hNumMicroslicesInMillislice",";Millislice;Number of Microslices;",16,100,116);
  hTimesADCGoesOverThreshold = new TH1I("hTimesADCGoesOverThreshold",";Times ADC Goes Over Threshold;",100,0,1000);
  hAsymmetry = new TProfile("hAsymmetry",";Channel;Asymmetry",2048,0,2048);
  // std::stringstream nameRCE;
  // for (unsigned int channel = 0; channel < 2048; channel++) {
  //   nameRCE.str(""); nameRCE << "ADCChannel"; nameRCE << channel;
  //   hADCChannel[channel] = new TH1I(nameRCE.str().c_str(),";Tick;",10000,0,10000);
  // }
  std::stringstream nameADCBit, nameADCBitAnd, nameADCBitOr;
  for (int chanPart = 0; chanPart < 4; chanPart++) {
    nameADCBit.str(""); nameADCBit << "hADCBits" << chanPart;
    hADCBits[chanPart] = new TH2I(nameADCBit.str().c_str(),";Channel;Bit",512,(chanPart*512),((chanPart+1)*512),16,0,16);
    nameADCBitAnd.str(""); nameADCBitAnd << "hADCBitsAnd" << chanPart;
    hADCBitsAnd[chanPart] = new TH2I(nameADCBitAnd.str().c_str(),";Channel;Bit",512,(chanPart*512),((chanPart+1)*512),16,0,16);
    nameADCBitOr.str(""); nameADCBitOr << "hADCBitsOr" << chanPart;
    hADCBitsOr[chanPart] = new TH2I(nameADCBitOr.str().c_str(),";Channel;Bit",512,(chanPart*512),((chanPart+1)*512),16,0,16);
  }

  // SSP hists
  hAvWaveformChannelEvent = new TH2D("hAvWaveformChannelEvent",";Event;Channel",100,0,100,96,0,96);
  hSSPDNoiseChannel = new TProfile("hSSPDNoiseChannel",";Channel;DNoise",96,0,96);
  hTotalWaveformEvent = new TH1I("hTotalWaveformEvent",";Total Waveform;",100,0,100);
  hTotalSSPHitsEvent = new TH1I("hTotalSSPHits",";Total SSP Hits;",100,0,1000);
  hTotalSSPHitsChannel = new TH1I("hTotalSSPHitsChannel",";Channel;Total SSP Hits;",96,0,96);
  hTimesWaveformGoesOverThreshold = new TH1I("hTimesWaveformGoesOverThreshold",";Time Waveform Goes Over Threshold;",100,0,1000);
  std::stringstream nameSSP;
  // for (unsigned int channel = 0; channel < 96; channel++) {
  //   nameSSP.str(""); nameSSP << "WaveformChannel"; nameSSP << channel;
  //   hWaveformChannel[channel] = new TH1I(nameSSP.str().c_str(),";Tick;",500,0,500);
  // }

  // General
  hNumSubDetectorsPresent = new TH1I("hNumSubDetectorsPresent",";Number of Subdetectors;",25,0,25);
  hSizeOfFiles = new TH1I("hSizeOfFiles",";Run&Subrun;Size (bytes);",20,0,20);
  hSizeOfFilesPerEvent = new TH1D("hSizeOfFilesPerEvent",";Run&Subrun;Size (bytes/event);",20,0,20);
}

void lbne::OnlineMonitoring::analyze(art::Event const &evt) {

  fEventNumber = evt.event();
  fRun = evt.run();
  fSubrun = evt.subRun();

  if (_verbose)
    std::cout << "Event number " << fEventNumber << std::endl;

  // Look for RCE millislice fragments
  art::Handle<artdaq::Fragments> rawRCE;
  evt.getByLabel("daq","TPC",rawRCE);
 
  // Look for SSP data
  art::Handle<artdaq::Fragments> rawSSP;
  evt.getByLabel("daq","PHOTON",rawSSP);

  // Check the data exists before continuing
  try { rawRCE->size(); }
  catch(std::exception e) { fIsRCE = false; }
  try { rawSSP->size(); }
  catch(std::exception e) { fIsSSP = false; }

  reset();

  // Analyse data separately
  if (fIsRCE) {
    analyzeRCE(rawRCE);
    windowingRCE();
    if (fADC.size()) monitoringRCE();
  }
  if (fIsSSP) {
    analyzeSSP(rawSSP);
    if (fWaveform.size()) monitoringSSP();
  }
  monitoringGeneral();

  // Event display -- every 500 events (8 s)
  if (fEventNumber % 500 == 0)
    eventDisplay();

}

void lbne::OnlineMonitoring::monitoringGeneral() {

  hNumSubDetectorsPresent->Fill(fNumSubDetectorsPresent);

  // Size of files
  if (fEventNumber == 1) {
    std::multimap<Long_t,std::pair<std::vector<TString>,Long_t>,std::greater<Long_t> >fileMap;
    const TString delimiter("_");

    TSystemDirectory dataDir("dataDir",fDataDirName);
    const TList *files = dataDir.GetListOfFiles();
    if (files) {
      TSystemFile *file;
      TString fileName;
      Long_t id,modified,size,flags;
      TIter next(files);
      while ( (file = (TSystemFile*)next()) ) {
	fileName = file->GetName();
    	if ( !file->IsDirectory() && fileName.EndsWith(".root") && !fileName.BeginsWith("lbne_r-") ) {
	  const TString path = fDataDirName+TString(file->GetName());
    	  gSystem->GetPathInfo(path.Data(),&id,&size,&flags,&modified);
	  TObjArray *splitName = path.Tokenize(delimiter);
	  if (splitName->GetEntriesFast() == 1)
	    continue;
	  std::vector<TString> name = {path,TString(splitName->At(1)->GetName()),TString(splitName->At(2)->GetName())};
	  fileMap.insert(std::pair<Long_t,std::pair<std::vector<TString>,Long_t> >(modified,std::pair<std::vector<TString>,Long_t>(name,size)));
    	}
      }
    }
    int i = 0;
    for(std::multimap<Long_t,std::pair<std::vector<TString>,Long_t> >::iterator mapIt = fileMap.begin(); mapIt != fileMap.end(); ++mapIt) {
      TString name = mapIt->second.first.at(1)+mapIt->second.first.at(2);
      if (_verbose)
	std::cout << "File: modified... " << mapIt->first << ", run " << name << " and size: " << mapIt->second.second << std::endl;
      ++i;
      std::stringstream cmd; cmd << "TFile::Open(\"" << mapIt->second.first.at(0) << "\"); Events->GetEntriesFast()";
      Int_t entries = gROOT->ProcessLine(cmd.str().c_str());
      hSizeOfFiles->GetXaxis()->SetBinLabel(i,name);
      hSizeOfFilesPerEvent->GetXaxis()->SetBinLabel(i,name);
      hSizeOfFiles->Fill(name,mapIt->second.second);
      hSizeOfFilesPerEvent->Fill(name,(double)((double)mapIt->second.second/(double)entries));
      if (i == 20) break;
    }
  }

}

void lbne::OnlineMonitoring::monitoringRCE() {
  // Function called to fill all histograms

  for (unsigned int channel = 0; channel < fADC.size(); channel++) {

    bool peak = false;
    int tTotalRCEHitsChannel = 0;

    // Find the mean ADC for this channel
    double sum = std::accumulate(fADC.at(channel).begin(),fADC.at(channel).end(),0.0);
    double mean = sum / fADC.at(channel).size();

    // Asymmetry variables
    double asymmetry = 0;
    double ADCsum = 0, ADCdiff = 0;

    for (unsigned int tick = 0; tick < fADC.at(channel).size(); tick++) {

      // Fill hists for tick
      //hADCChannel[channel]->Fill(tick,fADC.at(channel).at(tick));      
      if (channel) hRCEDNoiseChannel->Fill(channel,fADC.at(channel).at(tick)-fADC.at(channel-1).at(tick));

      // Increase variables
      fTotalADC += fADC.at(channel).at(tick);
      if (fADC.at(channel).at(tick) > fThreshold) {
	++tTotalRCEHitsChannel;
	++fTotalRCEHitsEvent;
      }

      // Times over threshold
      if ( (fADC.at(channel).at(tick) > fThreshold) && !peak ) {
	++fTimesADCGoesOverThreshold;
	peak = true;
      }
      if ( tick && (fADC.at(channel).at(tick) < fADC.at(channel).at(tick-1)) && peak ) peak = false;

      // // Bit check - would like to write as class variable and not define each time but can't...
      // std::bitset<16> bits(fADC.at(channel).at(tick));
      // for (int bit = 0; bit < 16; bit++) {
      // 	hADCBits[(int) (channel/512)]->Fill(channel,bit,bits[bit]);
      // 	hADCBitsAnd[(int) (channel/512)]->Fill(channel,bit,( (hADCBitsAnd[(int)(channel/512)]->GetBinContent(channel,bit)) && bits[bit] ));
      // 	hADCBitsOr[(int) (channel/512)]->Fill(channel,bit,( (hADCBitsOr[(int)(channel/512)]->GetBinContent(channel,bit)) || bits[bit] ));
      // }

    }

    // Fill hists for channel
    hAvADCChannelEvent->Fill(fEventNumber,channel,mean);
    hTotalRCEHitsChannel->Fill(channel+1,tTotalRCEHitsChannel);

    // Loop over blocks to look at the asymmetry
    for (int block = 0; block < fWindowingNumBlocks.at(channel); block++) {
      // Loop over the ticks within the block
      for (int tick = fWindowingBlockBegin.at(channel).at(block); tick < fWindowingBlockBegin.at(channel).at(block)+fWindowingBlockSize.at(channel).at(block); tick++) {
	if (fIsInduction) {
	  ADCdiff += fADC.at(channel).at(tick);
	  ADCsum += abs(fADC.at(channel).at(tick));
	}
      } // End of tick loop
    } // End of block loop

    if (fIsInduction && ADCsum) asymmetry = (double)ADCdiff / (double)ADCsum;
    hAsymmetry->Fill(channel+1,asymmetry);

  }

  // Fill hists for event
  hTotalADCEvent->Fill(fTotalADC);
  hTotalRCEHitsEvent->Fill(fTotalRCEHitsEvent);
  hTimesADCGoesOverThreshold->Fill(fTimesADCGoesOverThreshold);

}

void lbne::OnlineMonitoring::monitoringSSP() {
  // Function called to fill all histograms

  for (unsigned int channel = 0; channel < fWaveform.size(); channel++) {

    bool peak = false;
    int tTotalSSPHitsChannel = 0;

    // Find the mean Waveform for this channel
    double sum = std::accumulate(fWaveform.at(channel).begin(),fWaveform.at(channel).end(),0.0);
    double mean = sum / fWaveform.at(channel).size();

    for (unsigned int tick = 0; tick < fWaveform.at(channel).size(); tick++) {

      // Fill hists for tick
      //hWaveformChannel[channel]->Fill(tick,fWaveform.at(channel).at(tick));
      if (channel) hSSPDNoiseChannel->Fill(channel,fWaveform.at(channel).at(tick)-fWaveform.at(channel-1).at(tick));

      // Increase variables
      fTotalWaveform += fWaveform.at(channel).at(tick);
      ++fTotalSSPHitsEvent;
      ++tTotalSSPHitsChannel;

      // Times over threshold
      if ( (fWaveform.at(channel).at(tick) > fThreshold) && !peak ) {
	++fTimesWaveformGoesOverThreshold;
	peak = true;
      }
      if ( tick && (fWaveform.at(channel).at(tick) < fWaveform.at(channel).at(tick-1)) && peak ) peak = false;

    }

    // Fill hists for channel
    hAvWaveformChannelEvent->Fill(fEventNumber,channel,mean);
    hTotalSSPHitsChannel->Fill(channel+1,tTotalSSPHitsChannel);
    
  }

  // Fill hists for event
  hTotalWaveformEvent->Fill(fTotalWaveform);
  hTotalSSPHitsEvent->Fill(fTotalSSPHitsEvent);
  hTimesWaveformGoesOverThreshold->Fill(fTimesWaveformGoesOverThreshold);

}

void lbne::OnlineMonitoring::eventDisplay() {

  // Called every set number of events to display more detailed information about
  // the events being looked at

  

}

void lbne::OnlineMonitoring::analyzeRCE(art::Handle<artdaq::Fragments> rawRCE) {

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawRCE->size() << " fragment(s) of type RCE" << std::endl;

  fNumSubDetectorsPresent += rawRCE->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  tpcFragmentMap.clear();
  for (unsigned int fragIt = 0; fragIt < rawRCE->size(); fragIt++) {
    const artdaq::Fragment &fragment = ((*rawRCE)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID(); //+ 100;
    tpcFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
  }

  // Loop over channels
  for (unsigned int chanIt = 0; chanIt < 2048; chanIt++) {

    // Vector of ADC values for this channel
    std::vector<int> adcVector;

    // Find the fragment ID and the sample for this channel
    unsigned int fragmentID = (unsigned int)((chanIt/128)+100);
    unsigned int sample = (unsigned int)(chanIt%128);

    if (tpcFragmentMap.find(fragmentID) == tpcFragmentMap.end() )
      continue;

    if (_verbose)
      std::cout << "%%DQM---------- Channel " << chanIt << ", fragment " << fragmentID << " and sample " << sample << std::endl;

    // Find the millislice fragment this channel lives in
    unsigned int fragIndex = tpcFragmentMap[fragmentID];

    // Get the millislice fragment
    const artdaq::Fragment &frag = ((*rawRCE)[fragIndex]);
    lbne::TpcMilliSliceFragment millisliceFragment(frag);

    // Number of microslices in millislice fragments
    auto nMicroSlices = millisliceFragment.microSliceCount();

    hNumMicroslicesInMillislice->SetBinContent(fragmentID,nMicroSlices);

    if (_verbose)
      std::cout << "%%DQM--------------- TpcMilliSlice fragment " << fragmentID << " contains " << nMicroSlices << " microslices" << std::endl;

    for (unsigned int microIt = 0; microIt < nMicroSlices; microIt++) {

      // Get the microslice
      std::unique_ptr <const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
      auto nNanoSlices = microslice->nanoSliceCount();

      if (_daqtest)
	if (nNanoSlices > 0) std::cout << "Number of nanoslices... " << nNanoSlices << std::endl;

      if (_verbose)
	std::cout << "%%DQM-------------------- TpcMicroSlice " << microIt << " contains " << nNanoSlices << " nanoslices" << std::endl;

      for (unsigned int nanoIt = 0; nanoIt < nNanoSlices; nanoIt++) {

	// Get the ADC value
	uint16_t adc = std::numeric_limits<uint16_t>::max();
	bool success = microslice->nanosliceSampleValue(nanoIt, sample, adc);

	if (success) {
	  if (_daqtest)
	    if (adc > 0) std::cout << "adc = " << adc << std::endl;
	  adcVector.push_back((int)adc);
	}

      } // nanoslice loop

    } // microslice loop

    fADC.push_back(adcVector);

  } // channel loop

}

void lbne::OnlineMonitoring::analyzeSSP(art::Handle<artdaq::Fragments> rawSSP) {

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawSSP->size() << " fragment(s) of type PHOTON" << std::endl;

  fNumSubDetectorsPresent += rawSSP->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  sspFragmentMap.clear();
  for (unsigned int fragIt = 0; fragIt < rawSSP->size(); fragIt++) {
    const artdaq::Fragment &fragment = ((*rawSSP)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID();
    sspFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
  }

  // Keep a note of the previous fragment
  unsigned int prevFrag = 100;

  // Define a data pointer
  const unsigned int *dataPointer = 0;

  // Loop over optical channels
  for (unsigned int opChanIt = 0; opChanIt < 96; opChanIt++) {

    std::vector<int> waveformVector;

    // Find the fragment ID and the sample for this channel
    unsigned int fragmentID = (unsigned int)(opChanIt/12);
    unsigned int sample = (unsigned int)(opChanIt%12);

    // Check this fragment exists
    if (sspFragmentMap.find(fragmentID) == sspFragmentMap.end() )
      continue;

    if (_verbose)
      std::cout << "%%DQM---------- Optical Channel " << opChanIt << ", fragment " << fragmentID << " and sample " << sample << std::endl;

    // Find the fragment this channel lives in
    unsigned int fragIndex = sspFragmentMap[fragmentID];

    // Get the millislice fragment
    const artdaq::Fragment &frag = ((*rawSSP)[fragIndex]);
    lbne::SSPFragment sspFragment(frag);

    // Define at start of each fragment
    if (fragmentID != prevFrag) dataPointer = sspFragment.dataBegin();
    prevFrag = fragmentID;

    // Check there is data in this fragment
    if (dataPointer >= sspFragment.dataEnd())
      continue;

    // Get the event header
    const SSPDAQ::EventHeader *daqHeader = reinterpret_cast<const SSPDAQ::EventHeader*> (dataPointer);

    unsigned short OpChan = ((daqHeader->group2 & 0x000F) >> 0);
    unsigned int nADC = (daqHeader->length-sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int))*2;

    if (_verbose)
      std::cout << "%%DQM--------------- OpChan " << OpChan << ", number of ADCs " << nADC << std::endl;

    // Move data to end of trigger header (start of ADCs) and copy it
    dataPointer += sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
    const unsigned short *adcPointer = reinterpret_cast<const unsigned short*> (dataPointer);

    for (unsigned int adcIt = 0; adcIt < nADC; adcIt++) {
      const unsigned short *adc = adcPointer + adcIt;
      waveformVector.push_back(*adc);
    }

    // Move to end of trigger
    dataPointer += nADC/2;

    fWaveform.push_back(waveformVector);

  } // channel loop

}

void lbne::OnlineMonitoring::windowingRCE() {

  for (unsigned int channel = 0; channel < fADC.size(); channel++) {

    // Define variables for the windowing
    std::vector<short> tmpBlockBegin((fADC.at(channel).size()/2)+1);
    std::vector<short> tmpBlockSize((fADC.at(channel).size()/2)+1);
    int tmpNumBlocks = 0;
    int blockstartcheck = 0;
    int endofblockcheck = 0;

    for (int tick = 0; tick < (int)fADC.at(channel).size(); tick++) {
      // Windowing code (taken from raw.cxx::ZeroSuppression)
      if (blockstartcheck == 0) {
	if (fADC.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  if (tmpNumBlocks > 0) {
	    if (tick-fWindowingNearestNeighbour <= tmpBlockBegin[tmpNumBlocks-1] + tmpBlockSize[tmpNumBlocks-1]+1) {
	      tmpNumBlocks--;
	      tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	      blockstartcheck = 1;
	    }
	    else {
	      tmpBlockBegin[tmpNumBlocks] = (tick - fWindowingNearestNeighbour > 0) ? tick - fWindowingNearestNeighbour : 0;
	      tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	      blockstartcheck = 1;
	    }
	  }
	  else {
	    tmpBlockBegin[tmpNumBlocks] = (tick - fWindowingNearestNeighbour > 0) ? tick - fWindowingNearestNeighbour : 0;
	    tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	    blockstartcheck = 1;	    
	  }
	}
      }
      else if (blockstartcheck == 1) {
	if (fADC.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  ++tmpBlockSize[tmpNumBlocks];
	  endofblockcheck = 0;
	}
	else {
	  if (endofblockcheck < fWindowingNearestNeighbour) {
	    ++endofblockcheck;
	    ++tmpBlockSize[tmpNumBlocks];
	  }
	  //block has ended
	  else if ( std::abs(fADC.at(channel).at(tick+1)) <= fWindowingZeroThresholdSigned || std::abs(fADC.at(channel).at(tick+2)) <= fWindowingZeroThresholdSigned) {  
	    endofblockcheck = 0;
	    blockstartcheck = 0;
	    ++tmpNumBlocks;
	  }
	}
      }

    } // tick

    fWindowingBlockBegin.push_back(tmpBlockBegin);
    fWindowingBlockSize.push_back(tmpBlockSize);
    fWindowingNumBlocks.push_back(tmpNumBlocks);

  } // channel

}

void lbne::OnlineMonitoring::endJob() {
  cAvADCChannelEvent = new TCanvas("cAvADCChannelEvent","Average RCE ADC Value",800,600);
  hAvADCChannelEvent->Draw("colz");
  cAvADCChannelEvent->SaveAs("AvADCChannelEvent.png");

  cRCEDNoiseChannel = new TCanvas("cRCEDNoiseChannel","RCE DNoise",800,600);
  hRCEDNoiseChannel->Draw("colz");
  cRCEDNoiseChannel->SaveAs("RCEDNoiseChannel.png");

  cAvWaveformChannelEvent = new TCanvas("cAvWaveformChannelEvent","Average SSP ADC Value",800,600);
  hAvWaveformChannelEvent->Draw("colz");
  cAvWaveformChannelEvent->SaveAs("AvWaveformChannelEvent.png");

  cSSPDNoiseChannel = new TCanvas("cSSPDNoiseChannel","SSP DNoise",800,600);
  hSSPDNoiseChannel->Draw("colz");
  cSSPDNoiseChannel->SaveAs("SSPDNoiseChannel.png");

  cTotalADCEvent = new TCanvas("cTotalADCEvent","Total RCE ADC",800,600);
  hTotalADCEvent->Draw("colz");
  cTotalADCEvent->SaveAs("TotalADCEvent.png");

  cTotalWaveformEvent = new TCanvas("cTotalWaveformEvent","Total SSP ADC",800,600);
  hTotalWaveformEvent->Draw("colz");
  cTotalWaveformEvent->SaveAs("TotalWaveformEvent.png");

  cTotalRCEHitsEvent = new TCanvas("cTotalRCEHitsEvent","Total RCE Hits",800,600);
  hTotalRCEHitsEvent->Draw("colz");
  cTotalRCEHitsEvent->SaveAs("TotalRCEHitsEvent.png");

  cTotalSSPHitsEvent = new TCanvas("cTotalSSPHitsEvent","Total SSP Hits",800,600);
  hTotalSSPHitsEvent->Draw("colz");
  cTotalSSPHitsEvent->SaveAs("TotalSSPHitsEvent.png");

  cTotalRCEHitsChannel = new TCanvas("cTotalRCEHitsChannel","Total RCE Hits by Channel",800,600);
  hTotalRCEHitsChannel->Draw("colz");
  cTotalRCEHitsChannel->SaveAs("TotalRCEHitsChannel.png");

  cTotalSSPHitsChannel = new TCanvas("cTotalSSPHitsCharge","Total SSP Hits by Channel",800,600);
  hTotalSSPHitsChannel->Draw("colz");
  cTotalSSPHitsChannel->SaveAs("TotalSSPHitsChannel.png");

  cTimesADCGoesOverThreshold = new TCanvas("cTimesADCGoesOverThreshold","Times RCE ADC Over Threshold",800,600);
  hTimesADCGoesOverThreshold->Draw("colz");
  cTimesADCGoesOverThreshold->SaveAs("TimesADCGoesOverThreshold.png");

  cTimesWaveformGoesOverThreshold = new TCanvas("cTimesWaveformGoesOverThreshold","Times SSP ADC Over Threshold",800,600);
  hTimesWaveformGoesOverThreshold->Draw("colz");
  cTimesWaveformGoesOverThreshold->SaveAs("TimesWaveformGoesOverThreshold.png");

  cADCBits = new TCanvas("cADCBits","RCE ADC Bits Set",800,600);
  cADCBits->Divide(1,4);
  for (int chanPart = 0; chanPart < 4; chanPart++) {
    cADCBits->cd(4-chanPart);
    hADCBits[chanPart]->GetXaxis()->SetLabelSize(0.1);
    hADCBits[chanPart]->GetYaxis()->SetLabelSize(0.1);
    hADCBits[chanPart]->GetXaxis()->SetTitleSize(0.06);
    hADCBits[chanPart]->GetYaxis()->SetTitleSize(0.06);
    hADCBits[chanPart]->Draw("colz");
  }
  // fADCBits = cADCBits.DrawFrame(0.,0.,1.,1.);
  // fADCBits->SetXTitle("Channel");
  // fADCBits->SetYTitle("Bit");
  // cADCBits.Modified();
  cADCBits->SaveAs("ADCBits.png");

  cADCBitsAnd = new TCanvas("cADCBitsAnd","RCE ADC Bits Stuck Off",800,600);
  cADCBitsAnd->Divide(1,4);
  for (int chanPart = 0; chanPart < 4; chanPart++) {
    cADCBitsAnd->cd(4-chanPart);
    hADCBitsAnd[chanPart]->GetXaxis()->SetLabelSize(0.1);
    hADCBitsAnd[chanPart]->GetYaxis()->SetLabelSize(0.1);
    hADCBitsAnd[chanPart]->GetXaxis()->SetTitleSize(0.06);
    hADCBitsAnd[chanPart]->GetYaxis()->SetTitleSize(0.06);
    hADCBitsAnd[chanPart]->Draw();
  }
  cADCBitsAnd->SaveAs("ADCBitsAnd.png");

  cADCBitsOr = new TCanvas("cADCBitsOr","RCE ADC Bits Stuck On",800,600);
  cADCBitsOr->Divide(1,4);
  for (int chanPart = 0; chanPart < 4; chanPart++) {
    cADCBitsOr->cd(4-chanPart);
    hADCBitsOr[chanPart]->GetXaxis()->SetLabelSize(0.1);
    hADCBitsOr[chanPart]->GetYaxis()->SetLabelSize(0.1);
    hADCBitsOr[chanPart]->GetXaxis()->SetTitleSize(0.06);
    hADCBitsOr[chanPart]->GetYaxis()->SetTitleSize(0.06);
    hADCBitsOr[chanPart]->Draw();
  }
  cADCBitsOr->SaveAs("ADCBitsOr.png");

  cAsymmetry = new TCanvas("cAsymmetry","Asymmetry of Bipolar Induction Pulse",800,600);
  hAsymmetry->Draw();
  cAsymmetry->SaveAs("Asymmetry.png");

  cNumSubDetectorsPresent = new TCanvas("cNumSubDetectorsPresent","Number of Subdetectors",800,600);
  hNumSubDetectorsPresent->Draw();
  cNumSubDetectorsPresent->SetLogy();
  cNumSubDetectorsPresent->SaveAs("NumSubDetectorsPresent.png");

  cSizeOfFiles = new TCanvas("cSizeOfFiles","Data File Sizes",800,600);
  hSizeOfFiles->Draw();
  cSizeOfFiles->SaveAs("SizeOfFiles.png");

  cSizeOfFilesPerEvent = new TCanvas("cSizeOfFilesPerEvent","Data File Size Per Event",800,600);
  hSizeOfFilesPerEvent->Draw();
  pTitle->DeleteText();
  pTitle->AddText("Size of Files Per Event");
  pTitle->Draw();
  cSizeOfFilesPerEvent->SaveAs("SizeOfFilesPerEvent.png");

}

void lbne::OnlineMonitoring::reset() {

  fADC.clear();
  fWaveform.clear();

  fTotalADC = 0;
  fTotalWaveform = 0;

  fTotalRCEHitsEvent = 0;
  fTotalSSPHitsEvent = 0;

  fTimesADCGoesOverThreshold = 0;
  fTimesWaveformGoesOverThreshold = 0;

  fNumSubDetectorsPresent = 0;

}
 
DEFINE_ART_MODULE(lbne::OnlineMonitoring)
