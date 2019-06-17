#include "EventPacket.h"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

void SSPDAQ::EventPacket::SetEmpty(){
  data.clear();
  header.header=0xDEADBEEF;
}

void SSPDAQ::EventPacket::DumpHeader(){

  uint32_t peaksum = ((header.group3 & 0x00FF) >> 16) + header.peakSumLow;
  if(peaksum & 0x00800000) {
    peaksum |= 0xFF000000;
  }

  dune::DAQLogger::LogInfo("SSP_EventPacket")
    << "=====HEADER=======================================" << std::endl
    << "Header:                             " << header.header   << std::endl
    << "Length:                             " << header.length   << std::endl
    << "Trigger type:                       " << ((header.group1 & 0xFF00) >> 8) << std::endl
    << "Status flags:                       " << ((header.group1 & 0x00F0) >> 4) << std::endl
    << "Header type:                        " << ((header.group1 & 0x000F) >> 0) << std::endl
    << "Trigger ID:                         " << header.triggerID << std::endl
    << "Module ID:                          " << ((header.group2 & 0xFFF0) >> 4) << std::endl
    << "Channel ID:                         " << ((header.group2 & 0x000F) >> 0) << std::endl
    << "External timestamp (FP mode):       " << std::endl
    << "  Sync delay:                       " << ((unsigned int)(header.timestamp[1]) << 16) + (unsigned int)(header.timestamp[0]) << std::endl
    << "  Sync count:                       " << ((unsigned int)(header.timestamp[3]) << 16) + (unsigned int)(header.timestamp[2]) << std::endl
    << "External timestamp (NOvA mode):     " << ((unsigned long)header.timestamp[3]  << 48) +((unsigned long)header.timestamp[2] << 32)
    + ((unsigned long)header.timestamp[1] << 16) + (unsigned long)header.timestamp[0] <<std::endl
    << "Peak sum:                           " << peaksum << std::endl
    << "Peak time:                          " << ((header.group3 & 0xFF00) >> 8) << std::endl
    << "Prerise:                            " << ((header.group4 & 0x00FF) << 16) + header.preriseLow << std::endl
    << "Integrated sum:                     " << ((unsigned int)(header.intSumHigh) << 8) + (((unsigned int)(header.group4) & 0xFF00) >> 8) << std::endl
    << "Baseline:                           " << header.baseline << std::endl
    << "CFD Timestamp interpolation points: " << header.cfdPoint[0] << " " << header.cfdPoint[1] << " " << header.cfdPoint[2] << " " << header.cfdPoint[3] << std::endl
    << "Internal interpolation point:       " << header.intTimestamp[0] << std::endl
    << "Internal timestamp:                 " << ((uint64_t)((uint64_t)header.intTimestamp[3] << 32)) + ((uint64_t)((uint64_t)header.intTimestamp[2]) << 16) + ((uint64_t)((uint64_t)header.intTimestamp[1])) <<" ("<<header.intTimestamp[3]<<" "<<header.intTimestamp[2]<<" "<<header.intTimestamp[1]<<")"<<std::endl 
    << "=================================================="<< std::endl
    << std::endl;
}

void SSPDAQ::EventPacket::DumpEvent(){

  dune::DAQLogger::LogInfo("SSP_EventPacket")<<"*****EVENT DUMP***********************************" <<std::endl<<std::endl;

  this->DumpHeader();

  dune::DAQLogger::LogInfo("SSP_EventPacket")<<"=====ADC VALUES===================================" <<std::endl;

  unsigned int nADC=data.size()*2;
  unsigned short* adcs=reinterpret_cast<unsigned short*>(&(data[0]));

  std::stringstream adcstream;

  for(unsigned int i=0;i<nADC;++i){

    adcstream << adcs[i] << ", ";
  }

  dune::DAQLogger::LogInfo("SSP_EventPacket")<< adcstream.str() ;

  dune::DAQLogger::LogInfo("SSP_EventPacket")<<std::endl<<"**************************************************" 
					     <<std::endl<<std::endl;
}
