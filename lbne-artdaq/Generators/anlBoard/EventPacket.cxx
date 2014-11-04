#include "EventPacket.h"

void SSPDAQ::EventPacket::SetEmpty(){
  data.clear();
  header.header=0xDEADBEEF;
}
