////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: EventDisplay
// File: EventDisplay.hxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class with methods for making online event displays.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef EventDisplay_hxx
#define EventDisplay_hxx

#include "TH2D.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TColor.h"

#include "OnlineMonitoringBase.hxx"
#include "ChannelMap.hxx"
#include "DataReformatter.hxx"

// framework
#include "messagefacility/MessageLogger/MessageLogger.h"

class OnlineMonitoring::EventDisplay {
public:

  void MakeEventDisplay(RCEFormatter const& rceformatter, ChannelMap const& channelMap, int collectionPedestal, double driftVelocity);
  void SaveEventDisplay(int run, int subrun, int event, TString const& evdSavePath);
  int GetCollectionChannel(int offlineCollectionChannel, int apa, int drift);
  double GetZ(int collectionChannel);

private:

  TH2D* fEVD;

};

#endif
