////////////////////////////////////////////////////////////////////////
// Class:       OnlineMonitoring
// Module type: analyser
// File:        OnlineMonitoring_module.cc
// Author:      Mike Wallbank (m.wallbank@sheffield.ac.uk), April 2015
//
// Module to monitor data taking online for the 35t.
// Takes raw data from the DAQ and produces relevant monitoring data.
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
#include <ctime>
#include <typeinfo>
#include <thread>
#include <stdio.h>
#include <sys/stat.h>
#include <typeinfo>

#include "OnlineMonitoringBase.hxx"
#include "DataReformatter.hxx"
#include "MonitoringData.hxx"
#include "EventDisplay.hxx"
#include "ChannelMap.hxx"

class OnlineMonitoring::OnlineMonitoring : public art::EDAnalyzer {

public:

  OnlineMonitoring(fhicl::ParameterSet const& pset);

  void analyze(art::Event const& event);
  void beginSubRun(art::SubRun const& sr);
  void endSubRun(art::SubRun const& sr);
  void reconfigure(fhicl::ParameterSet const& p);

private:

  int fEventNumber;

  MonitoringData fMonitoringData;
  EventDisplay fEventDisplay;
  ChannelMap fChannelMap;

  // Monitoring options
  bool fDetailedMonitoring;
  bool fScopeMonitoring;

  // Data save info
  TString fDataDirPath;
  TString fMonitorSavePath;
  TString fEVDSavePath;
  TString fImageType;
  TString fChannelMapFile;

  // Refresh rates
  int fMonitoringRefreshRate;
  int fInitialMonitoringUpdate;
  int fEventDisplayRefreshRate;
  int fLastSaveTime;
  int fNEVDsMade;
  bool fSavedFirstMonitoring;

  // EVD creation
  int fMicroslicePreBuffer;
  int fMicrosliceTriggerLength;

  double fCollectionPedestal;
  double fDriftVelocity;

};

OnlineMonitoring::OnlineMonitoring::OnlineMonitoring(fhicl::ParameterSet const& pset) : EDAnalyzer(pset) {
  mf::LogInfo("Monitoring") << "Starting";
  this->reconfigure(pset);
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
}

void OnlineMonitoring::OnlineMonitoring::reconfigure(fhicl::ParameterSet const& p) {
  fDetailedMonitoring      = p.get<bool>("DetailedMonitoring");
  fScopeMonitoring         = p.get<bool>("ScopeMonitoring");
  fDataDirPath     = TString(p.get<std::string>("DataDirPath"));
  fMonitorSavePath = TString(p.get<std::string>("MonitorSavePath"));
  fEVDSavePath     = TString(p.get<std::string>("EVDSavePath"));
  fImageType       = TString(p.get<std::string>("ImageType"));
  fChannelMapFile  = TString(p.get<std::string>("ChannelMapFile"));
  fMonitoringRefreshRate   = p.get<int>("MonitoringRefreshRate");
  fInitialMonitoringUpdate = p.get<int>("InitialMonitoringUpdate");
  fEventDisplayRefreshRate = p.get<int>("EventDisplayRefreshRate");
  fDriftVelocity           = p.get<double>("DriftVelocity");
  fCollectionPedestal      = p.get<int>("CollectionPedestal");
  fMicroslicePreBuffer     = p.get<int>("MicroslicePreBuffer");
  fMicrosliceTriggerLength = p.get<int>("MicrosliceTriggerLength");
}

void OnlineMonitoring::OnlineMonitoring::beginSubRun(art::SubRun const& sr) {

  mf::LogInfo("Monitoring") << "Starting monitoring for run " << sr.run() << ", subRun " << sr.subRun();

  // Start a new monitoring run
  fMonitoringData.BeginMonitoring(sr.run(), sr.subRun(), fMonitorSavePath, fDetailedMonitoring, fScopeMonitoring);

  // Make the channel map for this subrun
  fChannelMap.MakeChannelMap(fChannelMapFile);

  // Monitoring data write out
  fLastSaveTime = std::time(0);
  fSavedFirstMonitoring = false;
  fNEVDsMade = 0;

}

void OnlineMonitoring::OnlineMonitoring::analyze(art::Event const& evt) {

  fEventNumber = evt.event();

  // Look for RCE millislice fragments
  art::Handle<artdaq::Fragments> rawRCE;
  evt.getByLabel("daq","TPC",rawRCE);

  // Look for SSP data
  art::Handle<artdaq::Fragments> rawSSP;
  evt.getByLabel("daq","PHOTON",rawSSP);

  // Look for PTB data
  art::Handle<artdaq::Fragments> rawPTB;
  evt.getByLabel("daq","TRIGGER",rawPTB);

  // Create data formatter objects and fill monitoring data products
  RCEFormatter rceformatter(rawRCE, fScopeMonitoring);
  SSPFormatter sspformatter(rawSSP);
  PTBFormatter ptbformatter(rawPTB);

  // Fill the data products in the monitoring data
  if (rawRCE.isValid()) {
    if (fScopeMonitoring) fMonitoringData.RCEScopeMonitoring(rceformatter, fEventNumber);
    else {
      fMonitoringData.RCEMonitoring(rceformatter, fEventNumber);
      if ((std::time(0) - fLastSaveTime) % 30 == 0) fMonitoringData.RCELessFrequentMonitoring(rceformatter);
    }
  }
  if (!fScopeMonitoring) {
    if (rawSSP.isValid()) fMonitoringData.SSPMonitoring(sspformatter);
    if (rawPTB.isValid()) fMonitoringData.PTBMonitoring(ptbformatter);
    fMonitoringData.GeneralMonitoring(rceformatter, sspformatter, ptbformatter, fDataDirPath);
    if (fDetailedMonitoring) fMonitoringData.FillTree(rceformatter, sspformatter);
  }

  // Write the data out every-so-often
  if ( (!fSavedFirstMonitoring and ((std::time(0) - fLastSaveTime) > fInitialMonitoringUpdate)) or ((std::time(0) - fLastSaveTime) > fMonitoringRefreshRate) ) {
    if (!fSavedFirstMonitoring) fSavedFirstMonitoring = true;
    fMonitoringData.WriteMonitoringData(evt.run(), evt.subRun(), fEventNumber, fImageType);
    fLastSaveTime = std::time(0);
  }

  // Make event display every-so-often
  // Eventually will check for flag in the PTB monitoring which suggests the event
  // is interesting enough to make an event display for!
  // if (ptbformatter.MakeEventDisplay())
  // int evdRefreshInterval = std::round((double)fEventDisplayRefreshRate / 1.6e-3);
  // if (fEventNumber % evdRefreshInterval == 0)
  if (fNEVDsMade == 0 and rceformatter.FirstMicroslice >= 2 and rceformatter.FirstMicroslice <= 5) {
    ++fNEVDsMade;
    rceformatter.AnalyseADCs(rawRCE, rceformatter.FirstMicroslice+fMicroslicePreBuffer, rceformatter.FirstMicroslice+fMicroslicePreBuffer+fMicrosliceTriggerLength);
    fEventDisplay.MakeEventDisplay(rceformatter, fChannelMap, fCollectionPedestal, fDriftVelocity);
    fEventDisplay.SaveEventDisplay(evt.run(), evt.subRun(), fEventNumber, fEVDSavePath);
  }

  // Consider detaching!
  // // Event display -- every 500 events (8 s)
  // if (fEventNumber % 20 == 0) {
  //   std::thread t(&OnlineMonitoring::eventDisplay,this,ADCs,Waveforms);
  //   t.detach();
  // }

  return;

}

void OnlineMonitoring::OnlineMonitoring::endSubRun(art::SubRun const& sr) {

  // Save the data at the end of the subrun
  fMonitoringData.WriteMonitoringData(sr.run(), sr.subRun(), fEventNumber, fImageType);

  // Clear up
  fMonitoringData.EndMonitoring();

}

DEFINE_ART_MODULE(OnlineMonitoring::OnlineMonitoring)
