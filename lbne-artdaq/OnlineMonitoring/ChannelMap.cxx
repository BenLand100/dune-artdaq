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
    int onlineChannel, rce, rcechannel, apa, drift, plane, offlineChannel;
    std::stringstream linestream(line);
    linestream >> onlineChannel >> rce >> rcechannel >> apa >> plane >> offlineChannel;
    if (rce == 0 or rce == 1 or rce == 4 or rce == 5 or rce == 8 or rce == 9 or rce == 12 or rce == 13) drift = 1;
    else drift = 0;
    fChannelMap[onlineChannel] = (std::unique_ptr<Channel>) new Channel(onlineChannel, offlineChannel, plane, apa, drift);
  }

  inFile.close();

}
