////////////////////////////////////////////////////////////////////////
//
// art module to produce online monitoring plots from raw artdaq data
// April 2015, M Wallbank (m.wallbank@sheffield.ac.uk)
//
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "artdaq-core/Data/Fragments.hh"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <numeric>
#include <bitset>
#include <cmath>
#include <typeinfo>
#include <thread>
#include <stdio.h>

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
#include <TMath.h>
#include <TFile.h>

typedef std::vector<std::vector<int> > DQMvector;

namespace lbne {
  class OnlineMonitoring;
}

class lbne::OnlineMonitoring : public art::EDAnalyzer {

public:

  explicit OnlineMonitoring(fhicl::ParameterSet const &pset);

  void      addHists();
  void      analyze(art::Event const &event);
  DQMvector analyzeRCE(art::Handle<artdaq::Fragments> rawRCE);
  DQMvector analyzeSSP(art::Handle<artdaq::Fragments> rawSSP);
  void      beginSubRun(const art::SubRun &sr);
  void      endSubRun(const art::SubRun &sr);
  void      eventDisplay(DQMvector ADCs, DQMvector Waveforms);
  void      monitoringGeneral();
  void      monitoringRCE(DQMvector ADCs);
  void      monitoringSSP(DQMvector Waveforms);
  void      reset();
  void      windowingRCE(DQMvector ADCs);

private:

  art::EventNumber_t fEventNumber;
  unsigned int fRun;
  unsigned int fSubrun;

  // File directories and paths
  const TString fDataDirName   = "/data/lbnedaq/data/";
  const TString fHistSavePath  = "/data/lbnedaq/scratch/wallbank/monitoring/";
  const TString fHistSaveType  = ".png";
  const TString fPathDelimiter = "_";

  // Run options
  bool _verbose = false;
  bool _daqtest = false;

  // SubRun flags
  bool fIsRCE, fIsSSP, receivedData, checkedFileSizes;

  int fThreshold = 10;
  bool fIsInduction = true;

  // Maps to track fragments which are present
  std::map<unsigned int, unsigned int> tpcFragmentMap;
  std::map<unsigned int, unsigned int> sspFragmentMap;

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

  // Monitoring histograms
  TObjArray fHistArray;
  TCanvas *fCanvas;

  TH1I *hTotalADCEvent, *hTotalRCEHitsEvent, *hTotalRCEHitsChannel, *hTimesADCGoesOverThreshold,  *hNumMicroslicesInMillislice, *hNumNanoslicesInMicroslice, *hNumNanoslicesInMillislice;
  TH2I *hBitCheckAnd, *hBitCheckOr;
  TH2D *hAvADCChannelEvent;
  TProfile *hADCMeanChannel, *hADCRMSChannel, *hRCEDNoiseChannel, *hAsymmetry;
  std::map<int,TProfile*> hADCChannelMap;

  TH1I *hTotalWaveformEvent, *hTotalSSPHitsEvent, *hTotalSSPHitsChannel, *hTimesWaveformGoesOverThreshold;
  TH2D *hAvWaveformChannelEvent;
  TProfile *hWaveformMeanChannel, *hWaveformRMSChannel, *hSSPDNoiseChannel;

  TH1I *hNumSubDetectorsPresent, *hSizeOfFiles;
  TH1D *hSizeOfFilesPerEvent;

};

lbne::OnlineMonitoring::OnlineMonitoring(fhicl::ParameterSet const &pset) : EDAnalyzer(pset) {
  mf::LogInfo("Monitoring") << "Starting";
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
}

void lbne::OnlineMonitoring::beginSubRun(const art::SubRun &sr) {

  mf::LogInfo("Monitoring") << "Creating new histograms for run " << sr.run() << ", subRun " << sr.subRun();

  // Set up new subrun
  fHistArray.Clear();
  fCanvas = new TCanvas("canv","",800,600);
  receivedData = false; checkedFileSizes = false; fIsRCE = true; fIsSSP = true;

  // Define all histograms
  // Nomenclature: Name is used to save the histogram, Title has format : histogramTitle_histDrawOptions_canvasOptions;x-title;y-title;z-title

  // RCE hists
  hADCMeanChannel             = new TProfile("ADCMeanChannel","RCE ADC Mean_\"histl\"_none;Channel;RCE ADC Mean",2048,0,2048);
  hADCRMSChannel              = new TProfile("ADCRMSChannel","RCE ADC RMS_\"histl\"_none;Channel;RCE ADC RMS",2048,0,2048);
  hAvADCChannelEvent          = new TH2D("AvADCChannelEvent","Average RCE ADC Value_\"colz\"_none;Event;Channel",100,0,100,2048,0,2048);
  hRCEDNoiseChannel           = new TProfile("RCEDNoiseChannel","RCE DNoise_\"colz\"_none;Channel;DNoise",2048,0,2048);
  hTotalADCEvent              = new TH1I("TotalADCEvent","Total RCE ADC_\"colz\"_logy;Total ADC;",100,0,1000000000);
  hTotalRCEHitsEvent          = new TH1I("TotalRCEHitsEvent","Total RCE Hits in Event_\"colz\"_logy;Total RCE Hits;",100,0,10000000);
  hTotalRCEHitsChannel        = new TH1I("TotalRCEHitsChannel","Total RCE Hits by Channel_\"colz\"_none;Channel;Total RCE Hits;",2048,0,2048);
  hNumMicroslicesInMillislice = new TH1I("NumMicroslicesInMillislice","Number of Microslices in Millislice_\"colz\"_none;Millislice;Number of Microslices;",16,100,116);
  hNumNanoslicesInMicroslice  = new TH1I("NumNanoslicesInMicroslice","Number of Nanoslices in Microslice_\"colz\"_logy;Number of Nanoslices;",1000,0,1100);
  hNumNanoslicesInMillislice  = new TH1I("NumNanoslicesInMillislice","Number of Nanoslices in Millislice_\"colz\"_logy;Number of Nanoslices;",1000,0,11000);
  hTimesADCGoesOverThreshold  = new TH1I("TimesADCGoesOverThreshold","Times RCE ADC Over Threshold_\"colz\"_none;Times ADC Goes Over Threshold;",100,0,1000);
  hAsymmetry                  = new TProfile("Asymmetry","Asymmetry of Bipolar Pulse_\"colz\"_none;Channel;Asymmetry",2048,0,2048);
  hBitCheckAnd                = new TH2I("BitCheckAnd","ADC Bits Always On_\"colz\"_none;Channel;Bit",2048,0,2048,16,0,16);
  hBitCheckOr                 = new TH2I("BitCheckOr","ADC Bits Always Off_\"colz\"_none;Channel;Bit",2048,0,2048,16,0,16);
  for (unsigned int channel = 0; channel < 2048; ++channel)
    hADCChannelMap[channel]   = new TProfile("ADCChannel"+TString(std::to_string(channel)),"ADC v Tick for Channel "+TString(std::to_string(channel))+";Tick;ADC;",5000,0,5000);

  // SSP hists
  hWaveformMeanChannel            = new TProfile("WaveformMeanChannel","SSP ADC Mean_\"histl\"_none;Channel;SSP ADC Mean",96,0,96);
  hWaveformRMSChannel             = new TProfile("WaveformRMSChannel","SSP ADC RMS_\"histl\"_none;Channel;SSP ADC RMS",96,0,96);
  hAvWaveformChannelEvent         = new TH2D("AvWaveformChannelEvent","Average SSP ADC Value_\"colz\"_none;Event;Channel",100,0,100,96,0,96);
  hSSPDNoiseChannel               = new TProfile("SSPDNoiseChannel","SSP DNoise_\"colz\"_none;Channel;DNoise",96,0,96);
  hTotalWaveformEvent             = new TH1I("TotalWaveformEvent","Total SSP ADC_\"colz\"_logy;Total Waveform;",100,0,1000000000);
  hTotalSSPHitsEvent              = new TH1I("TotalSSPHitsEvent","Total SSP Hits in Event_\"colz\"_logy;Total SSP Hits;",100,0,10000000);
  hTotalSSPHitsChannel            = new TH1I("TotalSSPHitsChannel","Total SSP Hits by Channel_\"colz\"_none;Channel;Total SSP Hits;",96,0,96);
  hTimesWaveformGoesOverThreshold = new TH1I("TimesWaveformGoesOverThreshold","Times SSP ADC Over Threshold_\"colz\"_none;Time Waveform Goes Over Threshold;",100,0,1000);

  // General
  hNumSubDetectorsPresent = new TH1I("NumSubDetectorsPresent","Number of Subdetectors_\"colz\"_logy;Number of Subdetectors;",25,0,25);
  hSizeOfFiles            = new TH1I("SizeOfFiles","Data File Sizes_\"colz\"_none;Run&Subrun;Size (bytes);",20,0,20);
  hSizeOfFilesPerEvent    = new TH1D("SizeOfFilesPerEvent","Size of Event in Data Files_\"colz\"_none;Run&Subrun;Size (bytes/event);",20,0,20);

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

  // Vectors to hold all information
  // channelvector<tickvector<ADC>>
  DQMvector ADCs;
  DQMvector Waveforms;

  // Format the data to channel/tick vectors
  if (fIsRCE) {
    ADCs = analyzeRCE(rawRCE);
    windowingRCE(ADCs);
  }    
  if (fIsSSP) Waveforms = analyzeSSP(rawSSP);

  // // Event display -- every 500 events (8 s)
  // if (fEventNumber % 20 == 0) {
  //   std::thread t(&OnlineMonitoring::eventDisplay,this,ADCs,Waveforms);
  //   t.detach();
  // }

  if (ADCs.size()) monitoringRCE(ADCs);
  if (Waveforms.size()) monitoringSSP(Waveforms);
  monitoringGeneral();

}

void lbne::OnlineMonitoring::monitoringGeneral() {

  hNumSubDetectorsPresent->Fill(fNumSubDetectorsPresent);

  // Size of files
  if (!checkedFileSizes) {
    checkedFileSizes = true;
    std::multimap<Long_t,std::pair<std::vector<TString>,Long_t>,std::greater<Long_t> > fileMap;

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
	  TObjArray *splitName = path.Tokenize(fPathDelimiter);
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

// Function called to fill all histograms
void lbne::OnlineMonitoring::monitoringRCE(DQMvector ADCs) {

  for (unsigned int channel = 0; channel < ADCs.size(); channel++) {

    if (!ADCs.at(channel).size())
      continue;

    // Variables for channel
    bool peak = false;
    int tTotalRCEHitsChannel = 0;
    bool tBitCheckAnd = 0xFFFF, tBitCheckOr = 0;
    double tAsymmetry = 0;
    double tADCsum = 0, tADCdiff = 0;

    // Find the mean and RMS of ADCs for this channel
    double mean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
    double rms  = TMath::RMS (ADCs.at(channel).begin(),ADCs.at(channel).end());

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); tick++) {

      // Fill hists for tick
      hADCChannelMap.at(channel)->Fill(tick,ADCs.at(channel).at(tick));
      if (channel) hRCEDNoiseChannel->Fill(channel,ADCs.at(channel).at(tick)-ADCs.at(channel-1).at(tick));

      // Increase variables
      fTotalADC += ADCs.at(channel).at(tick);
      if (ADCs.at(channel).at(tick) > fThreshold) {
	++tTotalRCEHitsChannel;
	++fTotalRCEHitsEvent;
      }

      // Times over threshold
      if ( (ADCs.at(channel).at(tick) > fThreshold) && !peak ) {
	++fTimesADCGoesOverThreshold;
	peak = true;
      }
      if ( tick && (ADCs.at(channel).at(tick) < ADCs.at(channel).at(tick-1)) && peak ) peak = false;

      // Bit check
      tBitCheckAnd &= ADCs.at(channel).at(tick);
      tBitCheckOr  |= ADCs.at(channel).at(tick);

    }

    // Fill hists for channel
    hADCMeanChannel     ->Fill(channel,mean);
    hADCRMSChannel      ->Fill(channel,rms);
    hAvADCChannelEvent  ->Fill(fEventNumber,channel,mean);
    hTotalRCEHitsChannel->Fill(channel+1,tTotalRCEHitsChannel);
    int tbit = 1;
    for (int bitIt = 0; bitIt < 16; ++bitIt) {
      hBitCheckAnd->Fill(channel,bitIt,(tBitCheckAnd & tbit));
      hBitCheckOr ->Fill(channel,bitIt,(tBitCheckOr & tbit));
      tbit <<= 1;
    }

    // Loop over blocks to look at the asymmetry
    for (int block = 0; block < fWindowingNumBlocks.at(channel); block++) {
      // Loop over the ticks within the block
      for (int tick = fWindowingBlockBegin.at(channel).at(block); tick < fWindowingBlockBegin.at(channel).at(block)+fWindowingBlockSize.at(channel).at(block); tick++) {
	if (fIsInduction) {
	  tADCdiff += ADCs.at(channel).at(tick);
	  tADCsum += abs(ADCs.at(channel).at(tick));
	}
      } // End of tick loop
    } // End of block loop

    if (fIsInduction && tADCsum) tAsymmetry = (double)tADCdiff / (double)tADCsum;
    hAsymmetry->Fill(channel+1,tAsymmetry);

  }

  // Fill hists for event
  hTotalADCEvent            ->Fill(fTotalADC);
  hTotalRCEHitsEvent        ->Fill(fTotalRCEHitsEvent);
  hTimesADCGoesOverThreshold->Fill(fTimesADCGoesOverThreshold);

}

// Function called to fill all histograms
void lbne::OnlineMonitoring::monitoringSSP(DQMvector Waveforms) {

  for (unsigned int channel = 0; channel < Waveforms.size(); channel++) {

    if (!Waveforms.at(channel).size())
      continue;

    bool peak = false;
    int tTotalSSPHitsChannel = 0;

    // Find the mean and RMS Waveform for this channel
    double mean = TMath::Mean(Waveforms.at(channel).begin(),Waveforms.at(channel).end());
    double rms  = TMath::RMS (Waveforms.at(channel).begin(),Waveforms.at(channel).end());

    for (unsigned int tick = 0; tick < Waveforms.at(channel).size(); tick++) {

      // Fill hists for tick
      if (channel) hSSPDNoiseChannel->Fill(channel,Waveforms.at(channel).at(tick)-Waveforms.at(channel-1).at(tick));

      // Increase variables
      fTotalWaveform += Waveforms.at(channel).at(tick);
      ++fTotalSSPHitsEvent;
      ++tTotalSSPHitsChannel;

      // Times over threshold
      if ( (Waveforms.at(channel).at(tick) > fThreshold) && !peak ) {
	++fTimesWaveformGoesOverThreshold;
	peak = true;
      }
      if ( tick && (Waveforms.at(channel).at(tick) < Waveforms.at(channel).at(tick-1)) && peak ) peak = false;

    }

    // Fill hists for channel
    hWaveformMeanChannel->Fill(channel,mean);
    hWaveformRMSChannel->Fill(channel,rms);
    hAvWaveformChannelEvent->Fill(fEventNumber,channel,mean);
    hTotalSSPHitsChannel->Fill(channel+1,tTotalSSPHitsChannel);
    
  }

  // Fill hists for event
  hTotalWaveformEvent->Fill(fTotalWaveform);
  hTotalSSPHitsEvent->Fill(fTotalSSPHitsEvent);
  hTimesWaveformGoesOverThreshold->Fill(fTimesWaveformGoesOverThreshold);

}

// void lbne::OnlineMonitoring::eventDisplay(DQMvector ADCs, DQMvector Waveforms) {

//   std::unique_ptr<TH2D> UMap = new TH2D("UMap",";Wire;Tick;",fGeometry->Nwires(0,0,0),0,fGeometry->Nwires(0,0,0),3200,0,3200);

//   for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
//     std::vector<geo::WireID> wires = fGeometry->ChannelToWire(channel)
//     for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
//       for (auto wire : wires) {
// 	if (fGeometry->View(channel) == geo::kU) {
// 	  UMap->Fill(wire.Wire,tick);
// 	}
//       }
//     }
//   }

//   for (unsigned int channel = 0; channel < Waveforms.size(); ++channel) {
//   }

//   UMap->delete();
// }

DQMvector lbne::OnlineMonitoring::analyzeRCE(art::Handle<artdaq::Fragments> rawRCE) {

  DQMvector ADCs;

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

	receivedData = true;

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

    ADCs.push_back(adcVector);

  } // channel loop

  if (!receivedData && fEventNumber%1000==0)
    mf::LogWarning("Monitoring") << "No nanoslices have been made after " << fEventNumber << " events";

  return ADCs;

}

DQMvector lbne::OnlineMonitoring::analyzeSSP(art::Handle<artdaq::Fragments> rawSSP) {

  DQMvector Waveforms;

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

    Waveforms.push_back(waveformVector);

  } // channel loop

  return Waveforms;

}

void lbne::OnlineMonitoring::windowingRCE(DQMvector ADCs) {

  for (unsigned int channel = 0; channel < ADCs.size(); channel++) {

    // Define variables for the windowing
    std::vector<short> tmpBlockBegin((ADCs.at(channel).size()/2)+1);
    std::vector<short> tmpBlockSize((ADCs.at(channel).size()/2)+1);
    int tmpNumBlocks = 0;
    int blockstartcheck = 0;
    int endofblockcheck = 0;

    for (int tick = 0; tick < (int)ADCs.at(channel).size(); tick++) {
      // Windowing code (taken from raw.cxx::ZeroSuppression)
      if (blockstartcheck == 0) {
	if (ADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
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
	if (ADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  ++tmpBlockSize[tmpNumBlocks];
	  endofblockcheck = 0;
	}
	else {
	  if (endofblockcheck < fWindowingNearestNeighbour) {
	    ++endofblockcheck;
	    ++tmpBlockSize[tmpNumBlocks];
	  }
	  //block has ended
	  else if ( std::abs(ADCs.at(channel).at(tick+1)) <= fWindowingZeroThresholdSigned || std::abs(ADCs.at(channel).at(tick+2)) <= fWindowingZeroThresholdSigned) {  
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

void lbne::OnlineMonitoring::endSubRun(art::SubRun const &sr) {

  // Add all histograms to the array for saving
  addHists();

  // Write the html for the web pages
  ofstream imageHTML((fHistSavePath+TString("index.html").Data()));

  // Make a root file with all the histograms in
  TFile *outFile = TFile::Open(fHistSavePath+TString("monitoringHistograms.root"),"RECREATE");

  // Save all the histograms as images and write to file
  for (int histIt = 0; histIt < fHistArray.GetEntriesFast(); ++histIt) {
    fCanvas->cd();
    TH1 *_h = (TH1*)fHistArray.At(histIt);
    TObjArray *histTitle = TString(_h->GetTitle()).Tokenize(fPathDelimiter);
    _h->Draw((char*)histTitle->At(1)->GetName());
    TPaveText *canvTitle = new TPaveText(0.05,0.92,0.6,0.98,"brNDC");
    canvTitle->AddText((std::string(histTitle->At(0)->GetName())+": Run "+std::to_string(fRun)+", SubRub "+std::to_string(fSubrun)).c_str());
    canvTitle->Draw();
    if (strstr(histTitle->At(2)->GetName(),"logy")) fCanvas->SetLogy(1);
    else fCanvas->SetLogy(0);
    fCanvas->Modified();
    fCanvas->Update();
    fCanvas->SaveAs(fHistSavePath+TString(_h->GetName())+fHistSaveType);
    outFile->cd();
    _h->Write();
    imageHTML << "<img src=\"" << (TString(_h->GetName())+fHistSaveType).Data() << "\">" << std::endl;
  }

  // Write other histograms
  for (unsigned int channel = 0; channel < 2048; ++channel)
    hADCChannelMap.at(channel)->Write();

  mf::LogInfo("Monitoring") << "Saved monitoring for run " << sr.run() << ", subRun " << sr.subRun() << " in " << fHistSavePath;

  // Copy the images over to the web server
  std::ostringstream cmd;
  cmd << fHistSavePath.Data() << "monitoringJob.sh " << sr.run() << " " << sr.subRun() << " " << fHistSavePath.Data() << " &";
  system(cmd.str().c_str());

}

void lbne::OnlineMonitoring::reset() {

  fTotalADC = 0;
  fTotalWaveform = 0;

  fTotalRCEHitsEvent = 0;
  fTotalSSPHitsEvent = 0;

  fTimesADCGoesOverThreshold = 0;
  fTimesWaveformGoesOverThreshold = 0;

  fNumSubDetectorsPresent = 0;

}

// Add histograms to histogram array
void lbne::OnlineMonitoring::addHists() {
  fHistArray.Add(hSSPDNoiseChannel); fHistArray.Add(hWaveformMeanChannel); fHistArray.Add(hWaveformRMSChannel); fHistArray.Add(hAvWaveformChannelEvent); fHistArray.Add(hTotalSSPHitsChannel);
  fHistArray.Add(hTimesWaveformGoesOverThreshold); fHistArray.Add(hTotalWaveformEvent); fHistArray.Add(hTotalSSPHitsEvent);
  fHistArray.Add(hNumMicroslicesInMillislice); fHistArray.Add(hRCEDNoiseChannel); fHistArray.Add(hADCMeanChannel); fHistArray.Add(hADCRMSChannel); fHistArray.Add(hAvADCChannelEvent); fHistArray.Add(hTotalRCEHitsChannel);
  fHistArray.Add(hAsymmetry); fHistArray.Add(hTotalADCEvent); fHistArray.Add(hTotalRCEHitsEvent); fHistArray.Add(hTimesADCGoesOverThreshold);
  fHistArray.Add(hNumSubDetectorsPresent); fHistArray.Add(hSizeOfFiles); fHistArray.Add(hSizeOfFilesPerEvent);
  fHistArray.Add(hBitCheckAnd); fHistArray.Add(hBitCheckOr);
}
 
DEFINE_ART_MODULE(lbne::OnlineMonitoring)
