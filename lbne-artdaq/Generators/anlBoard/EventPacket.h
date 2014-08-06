#ifndef EVENTPACKET_H__
#define EVENTPACKET_H__

#include "anlTypes.h"
#include <vector>

namespace SSPDAQ{

  class EventPacket{
  public:
    // Event Processing Functions
    //    Event* Unpack();
    void SetEmpty();
    EventHeader header;
    std::vector<unsigned int> data;
  };
  
}//namespace
#endif
