////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Monitoring namespace
// Author: Mike Wallbank, m.wallbank@sheffield.ac.uk (September 2015)
//
// Contains global variables used by classes in the monitoring namespace.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef OnlineMonitoringNamespace
#define OnlineMonitoringNamespace

#include "dune-raw-data/Overlays/PennMicroSlice.hh"
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
  const unsigned int NRCEs           = 16;
  const unsigned int NSSPChannels    = 84;
  const unsigned int NSSPs           = 7;
  const int          EventRate       = 200; //Hz
  const double       SamplingPeriod  = 0.5; //us
  const TString      PathDelimiter = "_";

  //NFB: What is the sampling period? Isn't the millislice supposed to be 5ms long?
  //     that would be 0.005 s = 5000 us
  // MW: sample period is the length of a 'tick' -- one tick is 500 ns.

  namespace TypeSizes {
    static int const CounterWordSize = 104;
    static int const TriggerWordSize = 32;
  }

  namespace EVD {
    const int LowerZ = 0, UpperZ = 174;
    const int LowerX = -50, UpperX = 500;
  }

  // PTB trigger types
  namespace PTBTrigger {
    const std::vector<dune::PennMicroSlice::Payload_Trigger::trigger_type_t> Muon = {dune::PennMicroSlice::Payload_Trigger::TD,  // TSU EL-WU (bin 1)
										     dune::PennMicroSlice::Payload_Trigger::TC,  // TSU SU-NL
										     dune::PennMicroSlice::Payload_Trigger::TB,  // TSU NU-SL
										     dune::PennMicroSlice::Payload_Trigger::TA}; // BSU RM-CL (bin 4)

    const std::vector<dune::PennMicroSlice::Payload_Trigger::trigger_type_t> Calibration = {dune::PennMicroSlice::Payload_Trigger::C1,
											    dune::PennMicroSlice::Payload_Trigger::C2,
											    dune::PennMicroSlice::Payload_Trigger::C3,
											    dune::PennMicroSlice::Payload_Trigger::C4};
  }

  const double NNanoSecondsPerNovaTick = 15.625; //ns

  const std::vector<std::string> DAQComponents = {"RCE00","RCE01","RCE02","RCE03","RCE04","RCE05","RCE06","RCE07","RCE08","RCE09","RCE10","RCE11","RCE12","RCE13","RCE14","RCE15",
						  "SSP01","SSP02","SSP03","SSP04","SSP05","SSP06","SSP07",
						  "PTB"};

}

#endif
