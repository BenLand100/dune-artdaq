////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: ChannelMap
// File: ChannelMap.hxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Mini class to handle mapping beteen online and offline channels (will probably be replaced
// by something more robust (e.g. database methods) at some point...
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef ChannelMap_hxx
#define ChannelMap_hxx

#include <map>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "OnlineMonitoringBase.hxx"

// framework
#include "messagefacility/MessageLogger/MessageLogger.h"

struct OnlineMonitoring::Channel {
  Channel(int onlineChannel, int offlineChannel, int plane, int apa, int drift) {
    OnlineChannel = onlineChannel;
    OfflineChannel = offlineChannel;
    Plane = plane;
    APA = apa;
    Drift = drift;
  }
  int OnlineChannel;
  int OfflineChannel;
  int Plane;
  int APA;
  int Drift;
};

class OnlineMonitoring::ChannelMap {
public:

  ChannelMap();

  int GetAPA(int onlineChannel) const { return fChannelMap.at(onlineChannel)->APA; }
  int GetDriftVolume(int onlineChannel) const { return fChannelMap.at(onlineChannel)->Drift; }
  std::map<int,std::unique_ptr<Channel> > const& GetChannelMap() const { return fChannelMap; }
  int GetOfflineChannel(int onlineChannel) const { return fChannelMap.at(onlineChannel)->OfflineChannel; }
  int GetPlane(int onlineChannel) const { return fChannelMap.at(onlineChannel)->Plane; }
  int GetPedestal(int onlineChannel) const { return fPedestalMap.at(onlineChannel); }
  void MakeChannelMap(TString const& channelMapFile);
  void MakePedestalMap(TString const& pedestalMapFile);
  bool hasPedestalData() const { return fHasPedestal; }

private:

  std::map<int,std::unique_ptr<Channel> > fChannelMap;
  std::map<int,int> fPedestalMap;

  bool fHasPedestal;

};

#endif
