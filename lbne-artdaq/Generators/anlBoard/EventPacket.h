#ifndef EVENTPACKET_H__
#define EVENTPACKET_H__

#include "lbne-raw-data/Overlays/anlTypes.hh"
#include <vector>

namespace SSPDAQ{

  class EventPacket{
  public:

    //Move constructor 
    EventPacket(EventPacket&& rhs){
      data=std::move(rhs.data);
      header=rhs.header;
    }

    //Move assignment operator
    EventPacket& operator=(EventPacket&& rhs){
      data=std::move(rhs.data);
      header=rhs.header;
      return *this;
    }

    // Event Processing Functions
    //    Event* Unpack();
    void SetEmpty();

    EventHeader header;
    std::vector<unsigned int> data;
  };
  
}//namespace
#endif
