////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.hxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#ifndef DataReformatter_hxx
#define DataReformatter_hxx

#include "art/Framework/Principal/Handle.h"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <iostream>
#include <vector>
#include <map>
#include <bitset>

#include "OnlineMonitoringNamespace.cxx"

class OnlineMonitoring::RCEFormatter {
public:

  // Defualt constructor (may come in handy!)
  RCEFormatter() {}
  RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE);
  std::vector<std::vector<int> > const& ADCVector() const { return ADCs; }
  std::vector<int> const& NumBlocks() const { return fWindowingNumBlocks; }
  std::vector<std::vector<short> > const& BlockBegin() const { return fWindowingBlockBegin; }
  std::vector<std::vector<short> > const& BlockSize() const { return fWindowingBlockSize; }
  int NumRCEs() { return fNRCEs; }

private:

  void AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE);
  void Windowing();

  std::vector<std::vector<int> > ADCs;
  int fNRCEs;

  // Windowing
  std::vector<int> fWindowingNumBlocks;
  std::vector<std::vector<short> > fWindowingBlockBegin;
  std::vector<std::vector<short> > fWindowingBlockSize;

};

class OnlineMonitoring::SSPFormatter {
public:

  // Defualt constructor (may come in handy!)
  SSPFormatter() {}
  SSPFormatter(art::Handle<artdaq::Fragments> const& rawSSP);
  std::vector<std::vector<int> > const& ADCVector() const { return ADCs; }
  int NumSSPs() { return fNSSPs; }

private:

  std::vector<std::vector<int> > ADCs;
  int fNSSPs;

};

class OnlineMonitoring::PTBFormatter {
public:

  // Defualt constructor (may come in handy!)
  PTBFormatter() {}
  PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB);
  void AnalyzeCounter(int counter_index, double &activation_time, int &hit_rate) const;
  void AnalyzeMuonTrigger(int trigger_number, int &trigger_rate) const;
  int NumTriggers() const { return fMuonTriggerRates.size(); }

private:

  void CollectCounterBits(uint8_t* payload, size_t payload_size);
  void CollectMuonTrigger(uint8_t* payload, size_t payload_size);
  std::vector<std::bitset<TypeSizes::CounterWordSize> > fCounterBits;
  std::vector<int> fCounterTimes;
  std::vector<std::bitset<TypeSizes::TriggerWordSize> > fMuonTriggerBits;
  std::map<int,int> fMuonTriggerRates;
  std::vector<int> fMuonTriggerTimes;

};

#endif
