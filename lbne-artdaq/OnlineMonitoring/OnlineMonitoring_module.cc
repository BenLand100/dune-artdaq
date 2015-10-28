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
#include <typeinfo>
#include <thread>
#include <stdio.h>
#include <sys/stat.h>
#include <typeinfo>

#include "OnlineMonitoringBase.cxx"
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

  art::EventNumber_t fEventNumber;

  MonitoringData fMonitoringData;
  EventDisplay fEventDisplay;
  ChannelMap fChannelMap;

  bool fMakeTree;

  // Refresh rates
  int fMonitoringRefreshRate;
  int fEventDisplayRefreshRate;

};

OnlineMonitoring::OnlineMonitoring::OnlineMonitoring(fhicl::ParameterSet const& pset) : EDAnalyzer(pset) {
  mf::LogInfo("Monitoring") << "Starting";
  this->reconfigure(pset);
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
}

void OnlineMonitoring::OnlineMonitoring::reconfigure(fhicl::ParameterSet const& p) {
  fMonitoringRefreshRate = p.get<int>("MonitoringRefreshRate");
  fEventDisplayRefreshRate = p.get<int>("EventDisplayRefreshRate");
  fMakeTree = p.get<bool>("MakeTree");
}

void OnlineMonitoring::OnlineMonitoring::beginSubRun(art::SubRun const& sr) {

  mf::LogInfo("Monitoring") << "Starting monitoring for run " << sr.run() << ", subRun " << sr.subRun();

  // Start a new monitoring run
  fMonitoringData.BeginMonitoring(sr.run(), sr.subRun());

  // Make the channel map for this subrun
  fChannelMap.MakeChannelMap();

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

  fMonitoringData.StartEvent(fEventNumber, fMakeTree);

  // Create data formatter objects and fill monitoring data products
  RCEFormatter rceformatter(rawRCE);
  SSPFormatter sspformatter(rawSSP);
  PTBFormatter ptbformatter(rawPTB);

  // Fill the data products in the monitoring data
  if (rawRCE.isValid()) fMonitoringData.RCEMonitoring(rceformatter);
  if (rawSSP.isValid()) fMonitoringData.SSPMonitoring(sspformatter);
  if (rawPTB.isValid()) fMonitoringData.PTBMonitoring(ptbformatter);
  fMonitoringData.GeneralMonitoring(rceformatter, sspformatter, ptbformatter);
  if (fMakeTree) fMonitoringData.FillTree(rceformatter, sspformatter);

  // Write the data out every-so-often
  int eventRefreshInterval = std::round((double)fMonitoringRefreshRate / 1.6e-3);
  if (fEventNumber % eventRefreshInterval == 0)
    fMonitoringData.WriteMonitoringData(evt.run(), evt.subRun());

  // Make event display every-so-often
  // Eventually will check for flag in the PTB monitoring which suggests the event
  // is interesting enough to make an event display for!
  // if (ptbformatter.MakeEventDisplay())
  int evdRefreshInterval = std::round((double)fEventDisplayRefreshRate / 1.6e-3);
  if (fEventNumber % evdRefreshInterval == 0)
    fEventDisplay.MakeEventDisplay(rceformatter, fChannelMap, fEventNumber);

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
  fMonitoringData.WriteMonitoringData(sr.run(), sr.subRun());

  // Clear up
  fMonitoringData.EndMonitoring();

}

DEFINE_ART_MODULE(OnlineMonitoring::OnlineMonitoring)
