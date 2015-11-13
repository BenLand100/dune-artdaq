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
#include <iomanip>
#include <vector>
#include <map>
#include <bitset>
#include <utility>
#include "TMath.h"

#include "OnlineMonitoringBase.cxx"

class OnlineMonitoring::RCEFormatter {
public:

  // Defualt constructor (may come in handy!)
  RCEFormatter() {}
  RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE);
  std::vector<std::vector<int> > const& ADCVector() const { return ADCs; }
  std::vector<int> const& NumBlocks() const { return fWindowingNumBlocks; }
  std::vector<std::vector<short> > const& BlockBegin() const { return fWindowingBlockBegin; }
  std::vector<std::vector<short> > const& BlockSize() const { return fWindowingBlockSize; }

  int NumRCEs;
  std::vector<std::string> RCEsWithData;

private:

  void AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE);
  void Windowing();

  std::vector<std::vector<int> > ADCs;

  // Windowing
  std::vector<int> fWindowingNumBlocks;
  std::vector<std::vector<short> > fWindowingBlockBegin;
  std::vector<std::vector<short> > fWindowingBlockSize;

};

struct OnlineMonitoring::Trigger {
  Trigger();
  Trigger(int channel, unsigned int peaksum, unsigned int prerise, unsigned int integral, unsigned int pedestal, unsigned int nTicks, double mean, double rms, std::vector<int> adcVector, unsigned long timestamp) {
    Channel = channel;
    PeakSum = peaksum;
    Prerise = prerise;
    Integral = integral;
    Pedestal = pedestal;
    NTicks = nTicks;
    Mean = mean;
    RMS = rms;
    ADCs = adcVector;
    Timestamp = timestamp;
  }
  int Channel;
  unsigned int PeakSum, Prerise, Integral, Pedestal, NTicks;
  double Mean, RMS;
  unsigned long Timestamp;
  std::vector<int> ADCs;
};

class OnlineMonitoring::SSPFormatter {
public:

  // Defualt constructor (may come in handy!)
  SSPFormatter() {}
  SSPFormatter(art::Handle<artdaq::Fragments> const& rawSSP);
  std::vector<Trigger> Triggers() const { return fTriggers; }
  std::map<int,std::vector<Trigger> > ChannelTriggers() const { return fChannelTriggers; }

  int NumSSPs;
  std::vector<std::string> SSPsWithData;

private:

  //std::unique_ptr<Trigger> fTrigger;
  std::vector<Trigger> fTriggers;
  std::map<int,std::vector<Trigger> > fChannelTriggers;

};

class OnlineMonitoring::PTBFormatter {
public:

  // Defualt constructor (may come in handy!)
  PTBFormatter() {}
  PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB);
  void AnalyzeCounter(int counter_index, double &activation_time, int &hit_rate) const;
  void AnalyzeMuonTrigger(int trigger_number, int &trigger_rate) const;
  int NumTriggers() const { return fMuonTriggerRates.size(); }

  bool PTBData;

private:

  void CollectCounterBits(uint8_t* payload, size_t payload_size);
  void CollectMuonTrigger(uint8_t* payload, size_t payload_size);
  void ReverseBits(std::bitset<TypeSizes::TriggerWordSize> &bits);
  void ReverseBits(std::bitset<TypeSizes::CounterWordSize> &bits);
  void ReverseBits(std::bitset<8> &bits);


  std::vector<std::bitset<TypeSizes::CounterWordSize> > fCounterBits;
  std::vector<int> fCounterTimes;
  std::vector<std::bitset<TypeSizes::TriggerWordSize> > fMuonTriggerBits;
  std::map<int,int> fMuonTriggerRates;
  std::vector<int> fMuonTriggerTimes;

};

#endif
