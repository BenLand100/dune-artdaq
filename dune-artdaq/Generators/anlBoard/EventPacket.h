#ifndef EVENTPACKET_H__
#define EVENTPACKET_H__

#include "lbne-raw-data/Overlays/anlTypes.hh"
#include <vector>

namespace SSPDAQ{


//Simple bag of data but with implementation of move methods
//to allow efficient shifting around of data between containers
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
    
    //Copy constructor
    EventPacket(const EventPacket& rhs){
      data=rhs.data;
      header=rhs.header;
    }

    //Copy assignment operator
    EventPacket& operator=(const EventPacket& rhs){
      data=rhs.data;
      header=rhs.header;
      return *this;
    }

    EventPacket(){}

    //Clear data vector and set header word to 0xDEADBEEF
    void SetEmpty();

    void DumpHeader();

    void DumpEvent();

    EventHeader header;

    std::vector<unsigned int> data;
  };
  
}//namespace
#endif
