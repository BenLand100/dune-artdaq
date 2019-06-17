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
#include "dune-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "dune-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/PennMicroSlice.hh"

#include "artdaq-core/Data/Fragment.hh"

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <bitset>
#include <utility>
#include "TMath.h"

#include "OnlineMonitoringBase.hxx"

class OnlineMonitoring::RCEFormatter {
public:

  // Defualt constructor (may come in handy!)
  RCEFormatter() {}
  RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE, bool scopeMode);
  void AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE, int firstMicroslice, int lastMicroslice);
  std::vector<std::vector<int> > const& ADCVector() const { return fADCs; }
  std::vector<std::vector<int> > const& EVDADCVector() const { return fEVDADCs; }
  std::vector<std::vector<unsigned long> > const& TimestampVector() const { return fTimestamps; }
  uint32_t const& ScopeChannel() const { return fScopeChannel; }
  std::vector<int> const& NumBlocks() const { return fWindowingNumBlocks; }
  std::vector<std::vector<short> > const& BlockBegin() const { return fWindowingBlockBegin; }
  std::vector<std::vector<short> > const& BlockSize() const { return fWindowingBlockSize; }

  int NumRCEs;
  int FirstMicroslice;
  bool HasData;
  std::vector<std::string> RCEsWithData;

private:

  void AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE, bool scopeMode);
  void Windowing();

  std::vector<std::vector<int> > fADCs;
  std::vector<std::vector<int> > fEVDADCs;
  std::vector<std::vector<unsigned long> > fTimestamps;
  uint32_t fScopeChannel;

  // Windowing
  std::vector<int> fWindowingNumBlocks;
  std::vector<std::vector<short> > fWindowingBlockBegin;
  std::vector<std::vector<short> > fWindowingBlockSize;

};

struct OnlineMonitoring::Trigger {
  Trigger();
  Trigger(int channel,
	  unsigned int peaksum,
	  unsigned int prerise,
	  unsigned int integral,
	  unsigned int pedestal,
	  unsigned int nTicks,
	  double mean,
	  double rms,
	  std::vector<int> adcVector,
	  unsigned long timestamp) {
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
  std::vector<int> ADCs;
  unsigned long Timestamp;
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
  PTBFormatter() :
    fNTotalTicks(0) {}
  PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB);
  void AnalyzeCounter(uint32_t counter_index, dune::PennMicroSlice::Payload_Timestamp::timestamp_t &activation_time, double &hit_rate) const;
  double AnalyzeMuonTrigger(dune::PennMicroSlice::Payload_Trigger::trigger_type_t trigger_number) const;
  double AnalyzeCalibrationTrigger(dune::PennMicroSlice::Payload_Trigger::trigger_type_t trigger_number) const;
  double AnalyzeSSPTrigger() const;
  uint32_t NumPayloads() const { return fPayloadTypes.size(); }
  std::vector<dune::PennMicroSlice::Payload_Header::data_packet_type_t> Payloads() const { return fPayloadTypes; }
  long double GetTotalSeconds() { return fNTotalTicks * NNanoSecondsPerNovaTick/(1000*1000*1000); };
#ifdef OLD_CODE
  PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB, PTBTrigger const& previousTrigger);
  void AnalyzeCounter(int counter_index, unsigned long &activation_time, double &hit_rate) const;
  void AnalyzeMuonTrigger(int trigger_number, double &trigger_rate) const;
  void AnalyzeCalibrationTrigger(int trigger_number, double& trigger_rate) const;
  void AnalyzeSSPTrigger(double& trigger_rate) const;
  int NumPayloads() const { return fPayloadTypes.size(); }
  std::vector<unsigned int> Payloads() const { return fPayloadTypes; }
  long double GetTotalSeconds() { return fNTotalTicks * NNanoSecondsPerNovaTick/(1000*1000*1000); };
  PTBTrigger GetLastTrigger() const { return fPreviousTrigger; }
#endif

  bool PTBData;

private:

#ifdef OLD_CODE
  void CollectCounterBits(uint8_t* payload, size_t payload_size);
  void CollectMuonTrigger(uint8_t* payload, size_t payload_size, dune::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp);

#else
  void CollectCounterBits(dune::PennMicroSlice::Payload_Header *header,dune::PennMicroSlice::Payload_Counter *trigger);
  void CollectTrigger(dune::PennMicroSlice::Payload_Trigger *trigger);
#endif
#ifdef OLD_CODE

  // Counters
  std::vector<std::bitset<TypeSizes::CounterWordSize> > fCounterBits;
  std::vector<unsigned long> fCounterTimes;

  // Triggers
  std::vector<unsigned long> fMuonTriggerTimes;
  std::vector<std::bitset<TypeSizes::TriggerWordSize> > fMuonTriggerBits;
  std::map<int,int> fMuonTriggerRates;
  std::vector<std::bitset<TypeSizes::TriggerWordSize> > fCalibrationTriggerBits;
  std::map<int,int> fCalibrationTriggerRates;
  int fSSPTriggerRates;

  // Payloads
  std::vector<unsigned int> fPayloadTypes;
  long double fTimeSliceSize;
  unsigned long fNTotalTicks;

  PTBTrigger fPreviousTrigger;

#else
  // Counters
  // NFB: I don't think that it is necessary to keep track of the individual words
  //std::vector<std::bitset<TypeSizes::CounterWordSize> > fCounterBits;
  std::vector<dune::PennMicroSlice::Payload_Timestamp::timestamp_t> fCounterTimes;

  std::vector<dune::PennMicroSlice::Payload_Counter> fCounterWords;

  // Triggers and calibrations
  std::vector<dune::PennMicroSlice::Payload_Timestamp::timestamp_t> fMuonTriggerTimes;
  //std::vector<std::bitset<TypeSizes::TriggerWordSize> > fMuonTriggerBits;
  std::map<dune::PennMicroSlice::Payload_Trigger::trigger_type_t,int> fMuonTriggerRates;

  //std::vector<std::bitset<TypeSizes::TriggerWordSize> > fCalibrationTriggerBits;
  std::map<dune::PennMicroSlice::Payload_Trigger::trigger_type_t,int> fCalibrationTriggerRates;
  std::vector<dune::PennMicroSlice::Payload_Timestamp::timestamp_t> fCalibrationTriggerTimes;

  int fSSPTriggerRates;
  std::vector<dune::PennMicroSlice::Payload_Header::data_packet_type_t> fPayloadTypes;
  long double fTimeSliceSize;
  unsigned long fNTotalTicks;

  // NFB: Two vectors that contain all trigger types
  static const std::vector<dune::PennMicroSlice::Payload_Trigger::trigger_type_t> fMuonTriggerTypes;
  static const std::vector<dune::PennMicroSlice::Payload_Trigger::trigger_type_t> fCalibrationTypes;

#endif /*OLD_CODE*/

};

#endif
