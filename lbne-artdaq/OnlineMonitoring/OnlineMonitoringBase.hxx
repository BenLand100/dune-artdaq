////////////////////////////////////////////////////////////////////////////////////////////////////
// Online Monitoring namespace
// Author: Mike Wallbank, m.wallbank@sheffield.ac.uk (September 2015)
//
// Contains global variables used by classes in the monitoring namespace.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef OnlineMonitoringNamespace
#define OnlineMonitoringNamespace

#include "lbne-raw-data/Overlays/PennMicroSlice.hh"
#include "TString.h"
#include <bitset>

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
  //NFB: What is the sampling period? Isn't the millislice supposed to be 5ms long?
  //     that would be 0.005 s = 5000 us
  const double       SamplingPeriod  = 0.5; //us
  const TString PathDelimiter = "_";

  namespace TypeSizes {
    static int const CounterWordSize = 104;
    static int const TriggerWordSize = 32;
  }

  //NFB: Not sure if I understand this logic either.
  // the positions should not be hardcoded from the bit locations. One problem with doing this is
  // the risk of getting into trouble due to endianess. Better to cast and then use the lbne::PennMicroSlice::Payload_Counter
  // structure to get the exact data.
  namespace CounterPos {
    // Hard-code all the positions for each counter in one place
    // The numbers in each vector are the bit numbers corresponding to the counter position in the payload
    const std::vector<int> TSUWU    = {7,8,9,10,11,12,13,14,15,16};
    const std::vector<int> TSUEL    = {17,18,19,20,21,22,23,24,25,26};
    const std::vector<int> TSUExtra = {27,28,29,30};
    const std::vector<int> TSUNU    = {31,32,33,34,35,36};
    const std::vector<int> TSUSL    = {37,38,39,40,41,42};
    const std::vector<int> TSUNL    = {43,44,45,46,47,48};
    const std::vector<int> TSUSU    = {49,50,51,52,53,54};
    const std::vector<int> BSURM    = {55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70};
    const std::vector<int> BSUCU    = {71,72,73,74,75,76,77,78,79,80};
    const std::vector<int> BSUCL    = {81,82,83,84,85,86,87,88,89,90,91,92,93};
    const std::vector<int> BSURL    = {94,95,96,97,98,99,100,101,102,103};
  }

  namespace EVD {
    const int LowerZ = 0, UpperZ = 174;
    const int LowerX = -50, UpperX = 500;
  }



  // what is this needed for? The PTB hit ignore time is done on purpose.
  // It should not go into any calculations
  const long PTBHitIgnoreTime = 7; //In nova timestamp units.

  //The coversion between nova and normal time is 1 timestamp:(16.625) ns.
  // The ignore time needs to be 400ns so its calculated in nova units as (400)/57.1 = 7 ticks
  //const double NNanoSecondsPerNovaTick = 57.1;  //This value is used to calculate PTBHitIgnoreTime above

  //NFB: Don't understand the logic above. The ignore time should not be used at all
  // using it will obviously yield the wrong number of counter rates.
  const double NNanoSecondsPerNovaTick = 16.625;  //This value is used to calculate PTBHitIgnoreTime above


  //NFB: A full timestamp should be used in this pair or values will be misused.
  typedef std::pair<lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t, std::bitset<TypeSizes::TriggerWordSize> > PTBTrigger;

  const std::vector<std::string> DAQComponents = {"RCE00","RCE01","RCE02","RCE03","RCE04","RCE05","RCE06","RCE07","RCE08","RCE09","RCE10","RCE11","RCE12","RCE13","RCE14","RCE15",
						  "SSP01","SSP02","SSP03","SSP04","SSP05","SSP06","SSP07",
						  "PTB"};

}

#endif
