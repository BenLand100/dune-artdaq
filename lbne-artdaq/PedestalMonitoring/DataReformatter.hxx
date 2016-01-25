////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.hxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw pedestal data
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

#include "PedestalMonitoringBase.cxx"

class PedestalMonitoring::RCEFormatter {
public:

  // Defualt constructor (may come in handy!)
  RCEFormatter() {}
  RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE);
  std::vector<std::vector<int> > const& ADCVector() const { return ADCs; }
  int NumRCEs() { return fNRCEs; }

private:

  void AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE);

  std::vector<std::vector<int> > ADCs;
  int fNRCEs;

};

#endif