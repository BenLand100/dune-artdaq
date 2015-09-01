////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.hh
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#ifndef DataReformatter_h
#define DataReformatter_h

#include "art/Framework/Principal/Handle.h"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <iostream>
#include <vector>
#include <map>
#include <bitset>

typedef std::vector<std::vector<int> > DQMvector;


namespace OnlineMonitoring {

  namespace TypeSizes{
    static int const CounterWordSize = 128;
    static int const TriggerWordSize = 32;

  }

  void ReformatRCEBoardData(art::Handle<artdaq::Fragments>& rawRCE, DQMvector* RCEADCs);
  void ReformatSSPBoardData(art::Handle<artdaq::Fragments>& rawSSP, DQMvector* SSPADCs);
  void ReformatTriggerBoardData(art::Handle<artdaq::Fragments> rawPTB);
  void WindowingRCEData(DQMvector& ADCs, std::vector<int>* numBlocks, std::vector<std::vector<short> >* blocksBegin, std::vector<std::vector<short> >* blocksSize);

  class PTBFormatter{
    public:
      PTBFormatter(art::Handle<artdaq::Fragments> rawPTB);
      void AnalyzeCounter(int counter_index, double &activation_time, int &hit_rate) const;
      void AnalyzeMuonTrigger(int trigger_number, int &trigger_rate);

    private:
      void CollectCounterBits(uint8_t* payload, size_t payload_size);
      void CollectMuonTrigger(uint8_t* payload, size_t payload_size);
      std::vector<std::bitset<TypeSizes::CounterWordSize> > fCounterBits;
      std::vector<int> fCounterTimes;
      std::vector<std::bitset<TypeSizes::TriggerWordSize> > fMuonTriggerBits;
      std::map<int,int> fMuonTriggerRates;
      std::vector<int> fMuonTriggerTimes;

  };
}

#endif
