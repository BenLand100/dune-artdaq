////////////////////////////////////////////////////////////////////////
// Class:       OnlinePedestal
// Module type: analyser
// File:        OnlinePedestal_module.cc
// Author:      Gabriel Santucci (gabriel.santucci@stonybrook.edu), Sept. 2015 -
//              based on OnlinePedestal_module.cc by      
//              Mike Wallbank (m.wallbank@sheffield.ac.uk), April 2015
//
// Module to monitor Noise runs online for the 35t.
// Takes raw data from the DAQ and produces relevant monitoring data.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
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

#include "OnlineMonitoringBase.hxx"
#include "DataReformatter.hxx"
#include "MonitoringPedestal.hxx"
#include "EventDisplay.hxx"
#include "ChannelMap.cxx"

class OnlineMonitoring::OnlinePedestal : public art::EDAnalyzer {

public:

  explicit OnlinePedestal(fhicl::ParameterSet const& pset);

  void analyze(art::Event const& event);
  void beginSubRun(art::SubRun const& sr);
  void endSubRun(art::SubRun const& sr);
  void reconfigure(fhicl::ParameterSet const& p);

private:

  art::EventNumber_t fEventNumber;

  MonitoringPedestal fMonitoringPedestal;
  EventDisplay fEventDisplay;
  ChannelMap fChannelMap;

  bool fMakeTree;

  // Refresh rates
  int fMonitoringRefreshRate;
  int fEventDisplayRefreshRate;

  // MW: stuff I need to put here to make it compile!  It won't do anything... Needs looking at if we're using this module
  TString fMonitorSavePath;
  TString fEVDSavePath;
  TString fImageType;

};

OnlineMonitoring::OnlinePedestal::OnlinePedestal(fhicl::ParameterSet const& pset) : EDAnalyzer(pset) {
  mf::LogInfo("Monitoring") << "Starting";
  this->reconfigure(pset);
  gStyle->SetOptStat(0);
  gStyle->SetOptTitle(0);
}

void OnlineMonitoring::OnlinePedestal::reconfigure(fhicl::ParameterSet const& p) {
  fMonitoringRefreshRate = p.get<int>("MonitoringRefreshRate");
  fEventDisplayRefreshRate = p.get<int>("EventDisplayRefreshRate");
  fMakeTree = p.get<bool>("MakeTree");
}

void OnlineMonitoring::OnlinePedestal::beginSubRun(art::SubRun const& sr) {

  mf::LogInfo("Monitoring") << "Starting Pedestal/Noise monitoring for run " << sr.run() << ", subRun " << sr.subRun();

  // Start a new monitoring run
  fMonitoringPedestal.BeginMonitoring(sr.run(), sr.subRun());

  // Make the channel map for this subrun
  fChannelMap.MakeChannelMap();

}

void OnlineMonitoring::OnlinePedestal::analyze(art::Event const& evt) {

  fEventNumber = evt.event();

  // Look for RCE millislice fragments
  art::Handle<artdaq::Fragments> rawRCE;
  evt.getByLabel("daq","TPC",rawRCE);

  fMonitoringPedestal.StartEvent(fEventNumber, fMakeTree);

  // Create data formatter objects and fill monitoring data products
  RCEFormatter rceformatter(rawRCE);

  // Fill the data products in the monitoring data
  if (rawRCE.isValid()) fMonitoringPedestal.RCEMonitoring(rceformatter);
  //fMonitoringPedestal.GeneralMonitoring();
  if (fMakeTree) fMonitoringPedestal.FillTree(rceformatter);

  // Write the data out every-so-often
  int eventRefreshInterval = std::round((double)fMonitoringRefreshRate / 1.6e-3);
  if (fEventNumber % eventRefreshInterval == 0)
    fMonitoringPedestal.WriteMonitoringPedestal(evt.run(), evt.subRun());

  // Make event display every-so-often
  int evdRefreshInterval = std::round((double)fEventDisplayRefreshRate / 1.6e-3);
  if (fEventNumber % evdRefreshInterval == 0)
    fEventDisplay.MakeEventDisplay(rceformatter, fChannelMap, fEventNumber, fEVDSavePath);

  // Consider detaching!
  // // Event display -- every 500 events (8 s)
  // if (fEventNumber % 20 == 0) {
  //   std::thread t(&OnlineMonitoring::eventDisplay,this,ADCs,Waveforms);
  //   t.detach();
  // }

  return;

}

void OnlineMonitoring::OnlinePedestal::endSubRun(art::SubRun const& sr) {

  // Save the data at the end of the subrun
  fMonitoringPedestal.WriteMonitoringPedestal(sr.run(), sr.subRun());

  // Clear up
  fMonitoringPedestal.EndMonitoring();

}

DEFINE_ART_MODULE(OnlineMonitoring::OnlinePedestal)
