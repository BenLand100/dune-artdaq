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

#include "OnlineMonitoringBase.cxx"
#include "ChannelMap.hxx"
#include "DataReformatter.hxx"

class OnlineMonitoring::EventDisplay {
public:

  void MakeEventDisplay(RCEFormatter const& rceformatter, ChannelMap const& channelMap, int event);
  void SaveEventDisplay();
  int GetCollectionChannel(int offlineCollectionChannel, int apa, int drift);

private:

};

#endif
