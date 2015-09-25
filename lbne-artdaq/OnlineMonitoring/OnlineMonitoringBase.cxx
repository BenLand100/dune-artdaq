////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Monitoring namespace
// Author: Mike Wallbank, m.wallbank@sheffield.ac.uk (September 2015)
//
// Contains global variables used by classes in the monitoring namespace.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef OnlineMonitoringNamespace
#define OnlineMonitoringNamespace

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

  const unsigned int NRCEChannels    = 512;
  const unsigned int NRCEMillislices = 4;
  const unsigned int NSSPChannels    = 96;
  const std::vector<int> DebugChannels = {260, 278, 289, 290};

  // Paths
  // Note -- on the gateway machine the default paths here refer to directories constantly
  // being scanned by cron jobs to upload new monitoring to the web.
  // If wanting to test and develop code, please change the paths!
  const TString DataDirName   = "/lbne/data2/users/wallbank/";
  const TString HistSavePath  = "/lbne/app/users/wallbank/lbne-artdaq-base/workspace/monitoring/";
  const TString EVDSavePath   = "/lbne/app/users/wallbank/lbne-artdaq-base/workspace/eventDisplay/";
  const TString ImageType     = ".png";
  const TString PathDelimiter = "_";

  const bool _verbose = false;

  namespace TypeSizes {
    static int const CounterWordSize = 128;
    static int const TriggerWordSize = 32;
  }

}

#endif
