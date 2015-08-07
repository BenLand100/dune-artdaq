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
#include "artdaq-core/Data/Fragments.hh"

#include <iostream>
#include <vector>

typedef std::vector<std::vector<int> > DQMvector;

namespace OnlineMonitoring {

  void ReformatRCEBoardData(art::Handle<artdaq::Fragments>& rawRCE, DQMvector* RCEADCs);
  void ReformatSSPBoardData(art::Handle<artdaq::Fragments>& rawSSP, DQMvector* SSPADCs);
  void ReformatTriggerBoardData();
  void WindowingRCEData(DQMvector& ADCs, std::vector<int>* numBlocks, std::vector<std::vector<short> >* blocksBegin, std::vector<std::vector<short> >* blocksSize);

}

#endif
