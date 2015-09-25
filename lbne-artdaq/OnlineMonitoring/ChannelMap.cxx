////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: ChannelMap
// File: ChannelMap.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Mini class to handle mapping beteen online and offline channels (will probably be replaced
// by something more robust (e.g. database methods) at some point...
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ChannelMap.hxx"

void OnlineMonitoring::ChannelMap::MakeChannelMap() {

  /// Read in channel map from a text file and make maps

  std::ifstream inFile("/data/lbnedaq/scratch/wallbank/lbne-artdaq-base/lbne-artdaq/lbne-artdaq/OnlineMonitoring/detailedMap.txt", std::ios::in);
  std::string line;

  while (std::getline(inFile,line)) {
    int onlineChannel, rce, rcechannel, apa, plane, offlineChannel;
    std::stringstream linestream(line);
    linestream >> onlineChannel >> rce >> rcechannel >> apa >> plane >> offlineChannel;
    fChannelMap[onlineChannel] = (std::unique_ptr<Channel>) new Channel(onlineChannel, offlineChannel, plane, apa);
  }

  inFile.close();

}

