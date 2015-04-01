#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <vector>
#include <map>
#include <iostream>
#include <numeric>

#include <TH1.h>
#include <TH2.h>
#include <TProfile2D.h>
#include <TPad.h>
#include <TCanvas.h>
#include <TStyle.h>

namespace lbne {
  class OnlineMonitoring;
}

class lbne::OnlineMonitoring : public art::EDAnalyzer {

public:

  explicit OnlineMonitoring(fhicl::ParameterSet const &pset);
  virtual ~OnlineMonitoring();

  void analyze(art::Event const &event);
  void analyzeTPC(art::Handle<artdaq::Fragments> rawTPC);
  void analyzeSSP(art::Handle<artdaq::Fragments> rawSSP);
  void beginJob();
  void monitoringTPC();
  void monitoringSSP();
  void endJob();

private:

  art::EventNumber_t fEventNumber;
  unsigned int fRun;
  unsigned int fSubrun;

  std::map<unsigned int, unsigned int> tpcFragmentMap;
  std::map<unsigned int, unsigned int> sspFragmentMap;

  std::vector<std::vector<int> > fADC;
  std::vector<std::vector<int> > fWaveform;

  // Monitoring histograms
  TH2D *hAvADCChannelEvent;
  TH2D *hAvWaveformChannelEvent;
  TProfile2D *hADCTickChannel;
  TProfile2D *hTPCDNoiseTickChannel;
  TProfile2D *hWaveformTickChannel;
  TProfile2D *hSSPDNoiseTickChannel;

  TCanvas *cAvADCChannelEvent;
  TCanvas *cAvWaveformChannelEvent;
  TCanvas *cADCTickChannel;
  TCanvas *cTPCDNoiseTickChannel;
  TCanvas *cWaveformTickChannel;
  TCanvas *cSSPDNoiseTickChannel;

  bool _verbose = false;

  bool fIsTPC = true;
  bool fIsSSP = true;

};

lbne::OnlineMonitoring::OnlineMonitoring(fhicl::ParameterSet const &pset) : EDAnalyzer(pset) {
}

lbne::OnlineMonitoring::~OnlineMonitoring() {
}

void lbne::OnlineMonitoring::beginJob() {
  gStyle->SetOptStat(0);

  hAvADCChannelEvent = new TH2D("hAvADCChannelEvent",";Event;Channel",100,0,99,2048,0,2047);
  hADCTickChannel = new TProfile2D("hADCTickChannel",";Channel;Tick",2048,0,2047,3200,0,3199);
  hTPCDNoiseTickChannel = new TProfile2D("hTPCDNoiseTickChannel",";Channel;Tick",2048,0,2047,3200,0,3199);

  hAvWaveformChannelEvent = new TH2D("hAvWaveformChannelEvent",";Event;Waveform",100,0,99,96,0,95);
  hWaveformTickChannel = new TProfile2D("hWaveformTickChannel",";Channel;Tick",96,0,95,500,0,499);
  hSSPDNoiseTickChannel = new TProfile2D("hSSPDNoiseTickChannel",";Channel;Tick",96,0,95,500,0,499);
}

void lbne::OnlineMonitoring::analyze(art::Event const &evt) {

  fEventNumber = evt.event();
  fRun = evt.run();
  fSubrun = evt.subRun();

  // Look for TPC millislice fragments
  art::Handle<artdaq::Fragments> rawTPC;
  evt.getByLabel("daq","TPC",rawTPC);

  // Look for SSP data
  art::Handle<artdaq::Fragments> rawSSP;
  evt.getByLabel("daq","PHOTON",rawSSP);

  // Check the data exists before continuing
  try { rawTPC->size(); }
  catch(std::exception e) { fIsTPC = false; }
  try { rawSSP->size(); }
  catch(std::exception e) { fIsSSP = false; }

  fADC.clear();
  fWaveform.clear();

  //fIsSSP = false;

  // Analyse data separately
  if (fIsTPC) {
    analyzeTPC(rawTPC);
    monitoringTPC();
  }
  if (fIsSSP) {
    analyzeSSP(rawSSP);
    monitoringSSP();
  }

}

void lbne::OnlineMonitoring::analyzeTPC(art::Handle<artdaq::Fragments> rawTPC) {

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawTPC->size() << " fragment(s) of type TPC" << std::endl;

  // Loop over fragments to make a map to save the order the frags are saved in
  tpcFragmentMap.clear();
  for (unsigned int fragIt = 0; fragIt < rawTPC->size(); fragIt++) {
    const artdaq::Fragment &fragment = ((*rawTPC)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID();
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
    const artdaq::Fragment &frag = ((*rawTPC)[fragIndex]);
    lbne::TpcMilliSliceFragment millisliceFragment(frag);

    // Number of microslices in millislice fragments
    auto nMicroSlices = millisliceFragment.microSliceCount();

    if (_verbose)
      std::cout << "%%DQM--------------- TpcMilliSlice fragment " << fragmentID << " contains " << nMicroSlices << " microslices" << std::endl;

    for (unsigned int microIt = 0; microIt < nMicroSlices; microIt++) {

      // Get the microslice
      std::unique_ptr <const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
      auto nNanoSlices = microslice->nanoSliceCount();

      if (_verbose)
	std::cout << "%%DQM-------------------- TpcMicroSlice " << microIt << " contains " << nNanoSlices << " nanoslices" << std::endl;

      for (unsigned int nanoIt = 0; nanoIt < nNanoSlices; nanoIt++) {

	// Get the ADC value
	uint16_t adc = std::numeric_limits<uint16_t>::max();
	bool success = microslice->nanosliceSampleValue(nanoIt, sample, adc);

	if (success)
	  adcVector.push_back((int)adc);

      } // nanoslice loop

    } // microslice loop

    fADC.push_back(adcVector);

  } // channel loop

}

void lbne::OnlineMonitoring::analyzeSSP(art::Handle<artdaq::Fragments> rawSSP) {

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawSSP->size() << " fragment(s) of type PHOTON" << std::endl;

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

void lbne::OnlineMonitoring::monitoringTPC() {
  // Function called to fill all histograms

  for (unsigned int channel = 0; channel < fADC.size(); channel++) {

    // Find the mean ADC for this channel
    double sum = std::accumulate(fADC.at(channel).begin(),fADC.at(channel).end(),0.0);
    double mean = sum / fADC.at(channel).size();

    hAvADCChannelEvent->Fill(fEventNumber,channel,mean);

    for (unsigned int tick = 0; tick < fADC.at(channel).size(); tick++) {
      hADCTickChannel->Fill(channel,tick,fADC.at(channel).at(tick));
      if (tick) hTPCDNoiseTickChannel->Fill(channel,tick,fADC.at(channel).at(tick-1));
    }
  }
  
}

void lbne::OnlineMonitoring::monitoringSSP() {
  // Function called to fill all histograms

  for (unsigned int channel = 0; channel < fWaveform.size(); channel++) {

    // Find the mean Waveform for this channel
    double sum = std::accumulate(fWaveform.at(channel).begin(),fWaveform.at(channel).end(),0.0);
    double mean = sum / fWaveform.at(channel).size();

    hAvWaveformChannelEvent->Fill(fEventNumber,channel,mean);

    for (unsigned int tick = 0; tick < fWaveform.at(channel).size(); tick++) {
      hWaveformTickChannel->Fill(channel,tick,fWaveform.at(channel).at(tick));
      if (tick) hSSPDNoiseTickChannel->Fill(channel,tick,fWaveform.at(channel).at(tick-1));
    }
  }

}

void lbne::OnlineMonitoring::endJob() {
  cAvADCChannelEvent = new TCanvas("cAvADCChannelEvent","",800,600);
  hAvADCChannelEvent->Draw("colz");
  cAvADCChannelEvent->SaveAs("AvADCChannelEvent.png");

  cADCTickChannel = new TCanvas("cADCTickChannel","",800,600);
  hADCTickChannel->Draw("colz");
  cADCTickChannel->SaveAs("ADCTickChannel.png");

  cTPCDNoiseTickChannel = new TCanvas("cTPCDNoiseTickChannel","",800,600);
  hTPCDNoiseTickChannel->Draw("colz");
  cTPCDNoiseTickChannel->SaveAs("TPCDNoiseTickChannel.png");

  cAvWaveformChannelEvent = new TCanvas("cAvWaveformChannelEvent","",800,600);
  hAvWaveformChannelEvent->Draw("colz");
  cAvWaveformChannelEvent->SaveAs("AvWaveformChannelEvent.png");

  cWaveformTickChannel = new TCanvas("cWaveformTickChannel","",800,600);
  hWaveformTickChannel->Draw("colz");
  cWaveformTickChannel->SaveAs("WaveformTickChannel.png");

  cSSPDNoiseTickChannel = new TCanvas("cSSPDNoiseTickChannel","",800,600);
  hSSPDNoiseTickChannel->Draw("colz");
  cSSPDNoiseTickChannel->SaveAs("SSPDNoiseTickChannel.png");
}

DEFINE_ART_MODULE(lbne::OnlineMonitoring)
