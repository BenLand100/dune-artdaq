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

#include "OnlineMonitoringNamespace.cxx"

struct OnlineMonitoring::Channel {
  Channel(int onlineChannel, int offlineChannel, int plane, int apa) {
    OnlineChannel = onlineChannel;
    OfflineChannel = offlineChannel;
    Plane = plane;
    APA = apa;
  }
  int OnlineChannel;
  int OfflineChannel;
  int Plane;
  int APA;
};

class OnlineMonitoring::ChannelMap {
public:

  int GetAPA(int onlineChannel) const { return fChannelMap.at(onlineChannel)->APA; }
  std::map<int,std::unique_ptr<Channel> > const& GetChannelMap() const { return fChannelMap; }
  int GetOfflineChannel(int onlineChannel) const { return fChannelMap.at(onlineChannel)->OfflineChannel; }
  int GetPlane(int onlineChannel) const { return fChannelMap.at(onlineChannel)->Plane; }
  void MakeChannelMap();

private:

  std::map<int,std::unique_ptr<Channel> > fChannelMap;

};

#endif
