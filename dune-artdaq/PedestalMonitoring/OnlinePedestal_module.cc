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

#include "DataReformatter.hxx"
#include "MonitoringPedestal.hxx"

class PedestalMonitoring::OnlinePedestal : public art::EDAnalyzer {

public:

  explicit OnlinePedestal(fhicl::ParameterSet const& pset);

  void analyze(art::Event const& event);
  void beginSubRun(art::SubRun const& sr);
  void endSubRun(art::SubRun const& sr);
  void reconfigure(fhicl::ParameterSet const& p);

private:
  art::EventNumber_t fEventNumber;
  MonitoringPedestal fMonitoringPedestal;
  int already_analyzed = 0;
  int window;
};

PedestalMonitoring::OnlinePedestal::OnlinePedestal(fhicl::ParameterSet const& pset) : EDAnalyzer(pset) {
  mf::LogInfo("Monitoring") << "Starting";
  this->reconfigure(pset);
  gStyle->SetOptStat(1);
  gStyle->SetOptTitle(0);
  gStyle->SetOptFit(1111);
}

void PedestalMonitoring::OnlinePedestal::reconfigure(fhicl::ParameterSet const& p) {
  window = p.get<int>("WindowSize");
}

void PedestalMonitoring::OnlinePedestal::beginSubRun(art::SubRun const& sr) {
  mf::LogInfo("Monitoring") << "Starting Pedestal/Noise monitoring for run " << sr.run() << ", subRun " << sr.subRun();
  // Start a new monitoring run
  fMonitoringPedestal.BeginMonitoring(sr.run(), sr.subRun());
}

void PedestalMonitoring::OnlinePedestal::analyze(art::Event const& evt) {

  fEventNumber = evt.event();

  // Look for RCE millislice fragments
  art::Handle<artdaq::Fragments> rawRCE;
  evt.getByLabel("daq","TPC",rawRCE);
  fMonitoringPedestal.StartEvent(fEventNumber);
  // Create data formatter objects and fill monitoring data products
  RCEFormatter rceformatter(rawRCE);
  // Fill the data products in the monitoring data
  if (rawRCE.isValid() and already_analyzed < 1)
    already_analyzed += fMonitoringPedestal.RCEMonitoring(rceformatter, window);

  return;

}

void PedestalMonitoring::OnlinePedestal::endSubRun(art::SubRun const& sr) {
  // Clear up
  std::cout << "ending run, subrun = " << sr.run() << ", " << sr.subRun() << std::endl;
  fMonitoringPedestal.EndMonitoring();

  //add construction of database here, channel map + mean and RMS

}

DEFINE_ART_MODULE(PedestalMonitoring::OnlinePedestal)
