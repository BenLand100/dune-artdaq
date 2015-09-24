////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Monitoring namespace
// Author: Mike Wallbank, m.wallbank@sheffield.ac.uk (September 2015)
//
// Contains global variables used by classes in the monitoring namespace.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef OnlineMonitoringNamespace_cxx
#define OnlineMonitoringNamespace_cxx

#include "TString.h"

namespace OnlineMonitoring {

  class OnlineMonitoring;
  class OnlinePedestal;
  class RCEFormatter;
  class SSPFormatter;
  class PTBFormatter;
  class MonitoringData;
  class MonitoringPedestal;
  class EventDisplay;
  class ChannelMap;
  struct Channel;

  unsigned int NRCEChannels    = 512;
  unsigned int NRCEMillislices = 4;
  unsigned int NSSPChannels    = 96;
  std::vector<int> DebugChannels = {260, 278, 289, 290};

  // Paths
  // Note -- on the gateway machine the default paths here refer to directories constantly
  // being scanned by cron jobs to upload new monitoring to the web.
  // If wanting to test and develop code, please change the paths!
  const TString DataDirName   = "/data/lbnedaq/data/";
  const TString HistSavePath  = "/data/lbnedaq/monitoring/";
  const TString EVDSavePath   = "/data/lbnedaq/eventDisplay/";
  const TString ImageType     = ".png";
  const TString PathDelimiter = "_";
  TString HistSaveDirectory;

  bool _verbose = false;

  namespace TypeSizes {
    static int const CounterWordSize = 128;
    static int const TriggerWordSize = 32;
  }

}

#endif
