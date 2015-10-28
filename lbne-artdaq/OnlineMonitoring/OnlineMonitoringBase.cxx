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
  struct Trigger;

  const unsigned int NRCEChannels    = 2048;
  const unsigned int NRCEMillislices = 16;
  const unsigned int NSSPChannels    = 84;
  const std::vector<int> DebugChannels = {260, 278, 289, 290};

  // Paths
  // Note -- on the gateway machine the default paths here refer to directories constantly
  // being scanned by cron jobs to upload new monitoring to the web.
  // If wanting to test and develop code, please change the paths!
  const TString DataDirName   = "/storage/data/";
  const TString HistSavePath  = "/data2/lbnedaq/monitoring/";
  const TString EVDSavePath   = "/data2/lbnedaq/eventDisplay/";
  const TString ImageType     = ".png";
  const TString PathDelimiter = "_";

  namespace TypeSizes {
    static int const CounterWordSize = 128;
    static int const TriggerWordSize = 32;
  }

  const std::vector<std::string> DAQComponents = {"RCE00","RCE01","RCE02","RCE03","RCE04","RCE05","RCE06","RCE07","RCE08","RCE09","RCE10","RCE11","RCE12","RCE13","RCE14","RCE15",
						  "SSP01","SSP02","SSP03","SSP04","SSP05","SSP06","SSP07",
						  "PTB"};

}

#endif
