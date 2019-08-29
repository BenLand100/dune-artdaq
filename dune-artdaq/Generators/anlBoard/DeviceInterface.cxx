#include "DeviceInterface.h"
#include "anlExceptions.h"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "RegMap.h"
#include <time.h>
#include <utility>
#include "boost/asio.hpp"

SSPDAQ::DeviceInterface::DeviceInterface(SSPDAQ::Comm_t commType, unsigned long deviceId)
  : fCommType(commType), fDeviceId(deviceId), fState(SSPDAQ::DeviceInterface::kUninitialized),
    fUseExternalTimestamp(false), fHardwareClockRateInMHz(128), fPreTrigLength(1E8), 
    fPostTrigLength(1E7), fTriggerWriteDelay(1000), fTriggerLatency(0), fTriggerMask(0),
    fDummyPeriod(-1), fSlowControlOnly(false), fPartitionNumber(0), fTimingAddress(0), exception_(false),
    fDataThread(0), fRequestReceiver(0){
}

void SSPDAQ::DeviceInterface::OpenSlowControl(){
  //Ask device manager for a pointer to the specified device
  SSPDAQ::DeviceManager& devman=SSPDAQ::DeviceManager::Get();

  SSPDAQ::Device* device=0;

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Opening "<<((fCommType==SSPDAQ::kUSB)?"USB":((fCommType==SSPDAQ::kEthernet)?"Ethernet":"Emulated"))
		     <<" device #"<<fDeviceId<<" for slow control only..."<<std::endl;
  
  device=devman.OpenDevice(fCommType,fDeviceId,true);
  
  if(!device){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Unable to get handle to device; giving up!"<<std::endl;
    } catch (...) {}
      throw(ENoSuchDevice());
  }

  fDevice=device;
  fSlowControlOnly=true;
}

void SSPDAQ::DeviceInterface::Initialize(){

  //Ask device manager for a pointer to the specified device
  SSPDAQ::DeviceManager& devman=SSPDAQ::DeviceManager::Get();

  SSPDAQ::Device* device=0;

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Initializing "<<((fCommType==SSPDAQ::kUSB)?"USB":((fCommType==SSPDAQ::kEthernet)?"Ethernet":"Emulated"))
		     <<" device #"<<fDeviceId<<"..."<<std::endl;
  
  device=devman.OpenDevice(fCommType,fDeviceId);
  
  if(!device){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Unable to get handle to device; giving up!"<<std::endl;
    } catch (...) {}
      throw(ENoSuchDevice());
  }

  fDevice=device;

  //Put device into sensible state
  this->Stop();

  //Reset timing endpoint
  SSPDAQ::RegMap& duneReg=SSPDAQ::RegMap::Get();

  unsigned int pdts_status=0;
  unsigned int pdts_control=0;
  unsigned int dsp_clock_control=0;

  fDevice->DeviceRead(duneReg.pdts_status, &pdts_status);
  fDevice->DeviceRead(duneReg.pdts_control, &pdts_control);
  fDevice->DeviceRead(duneReg.dsp_clock_control, &dsp_clock_control);

  unsigned int presentTimingAddress=(pdts_control>>16)&0xFF;
  unsigned int presentTimingPartition=pdts_control&0x3;

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"SSP HW presently on partition "<< presentTimingPartition
						 <<", address "<<std::hex<<presentTimingAddress<<" with endpoint status "
						 <<(pdts_status&0xF)<<" and dsp_clock_control at "<<dsp_clock_control
						 <<std::dec<<std::endl;

  if((pdts_status&0xF)>=0x6 && (pdts_status&0xF)<=0x8 && presentTimingAddress==fTimingAddress && presentTimingPartition==fPartitionNumber
     && dsp_clock_control==0x31){

    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Clock already looks ok... skipping endpoint reset."<<std::endl;

  }
  else{
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Syncing SSP to PDTS (partition "<<fPartitionNumber
						   <<", endpoint address "<<std::hex<<fTimingAddress
						   <<std::dec<<")"<<std::endl;

    unsigned int nTries=0;
    
    while(nTries<5){
      fDevice->DeviceWrite(duneReg.dsp_clock_control,0x30);
      fDevice->DeviceWrite(duneReg.pdts_control, 0x80000000 + fPartitionNumber + fTimingAddress*0x10000);
      fDevice->DeviceWrite(duneReg.pdts_control, 0x00000000 + fPartitionNumber + fTimingAddress*0x10000);
      usleep(2000000);
      fDevice->DeviceWrite(duneReg.dsp_clock_control,0x31);
      usleep(2000000);
      fDevice->DeviceRead(duneReg.pdts_status, &pdts_status);
      if((pdts_status&0xF)>=0x6 && (pdts_status&0xF)<=0x8 ) break;
      dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Timing endpoint sync failed (try "<<nTries<<")"<<std::endl;
      ++nTries;
    }
   
    if((pdts_status&0xF)>=0x6 && (pdts_status&0xF)<=0x8 ){
      dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Timing endpoint synced!"<<std::endl;
    }
    else{
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Giving up on endpoint sync after 5 tries. Value of pdts_status register was "
						      <<std::hex<<pdts_status<<std::dec<<std::endl;
    } 
  }

  //Wait until pdts_status reaches exactly 0x8 before resolving.
  if((pdts_status&0xF)!=0x8){
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Waiting for endpoint to reach status 0x8..."<<std::endl;
  }
  while((pdts_status&0xF)!=0x8){
    usleep(2000000);
    fDevice->DeviceRead(duneReg.pdts_status, &pdts_status);
  }

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Endpoint is in running state, continuing with configuration!"<<std::endl;
}

void SSPDAQ::DeviceInterface::Stop(){
  if(fState!=SSPDAQ::DeviceInterface::kRunning&&
     fState!=SSPDAQ::DeviceInterface::kUninitialized){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Running stop command for non-running device!"<<std::endl;
  }

  if(fState==SSPDAQ::DeviceInterface::kRunning){
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Device interface stopping run"<<std::endl;
    fShouldStop=true;
    if(fRequestReceiver){
      fRequestReceiver->stop();
    }
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Signalling read thread to end..."<<std::endl;
    fDataThread->join();
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Read thread terminated!"<<std::endl;
  }


  SSPDAQ::RegMap& duneReg=SSPDAQ::RegMap::Get();
      
  fDevice->DeviceWrite(duneReg.eventDataControl, 0x0013001F);
  fDevice->DeviceClear(duneReg.master_logic_control, 0x00000101);
  // Clear the FIFOs
  fDevice->DeviceWrite(duneReg.fifo_control, 0x08000000);
  fDevice->DeviceWrite(duneReg.PurgeDDR, 0x00000001);
  // Reset the links and flags				
  fDevice->DeviceWrite(duneReg.event_data_control, 0x00020001);
  // Flush RX buffer
  fDevice->DevicePurgeData();
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Hardware set to stopped state"<<std::endl;

  if(fState==SSPDAQ::DeviceInterface::kRunning){
    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"DeviceInterface stop transition complete!"<<std::endl;
  }

  fState=SSPDAQ::DeviceInterface::kStopped;

}

void SSPDAQ::DeviceInterface::StartRequestReceiver(std::string address){

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Subscribing to software triggers at "<<address<<" and ignoring hardware triggers."<<std::endl;
  fRequestReceiver=new RequestReceiver(address);
}

void SSPDAQ::DeviceInterface::Start(){

  if(fState!=kStopped){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to start acquisition on non-stopped device refused!"<<std::endl;
    return;
  }
 
  if(fSlowControlOnly){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Naughty Zoot! Attempt to start run on slow control interface refused!"<<std::endl;
    } catch (...) {}
    return;
  }

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Device interface starting hardware run..."<<std::endl;
  SSPDAQ::RegMap& duneReg=SSPDAQ::RegMap::Get();
  // This script enables all logic and FIFOs and starts data acquisition in the device
  // Operations MUST be performed in this order
  
  //Load window settings, charge injection settings and bias voltage into channels
  fDevice->DeviceWrite(duneReg.channel_pulsed_control, 0x1);
  fDevice->DeviceWrite(duneReg.bias_control, 0x1);
  fDevice->DeviceWriteMask(duneReg.vmon_control, 0x1, 0x1);
  fDevice->DeviceWriteMask(duneReg.imon_control, 0x1, 0x1);
  fDevice->DeviceWriteMask(duneReg.qi_dac_control, 0x1, 0x1);
  fDevice->DeviceWriteMask(duneReg.qi_pulsed, 0x00030000, 0x00030000);
  
  fDevice->DeviceWrite(duneReg.event_data_control, 0x00000000);
  // Release the FIFO reset						
  fDevice->DeviceWrite(duneReg.fifo_control, 0x00000000);
  // Registers in the Zynq FPGA (Comm)
  // Reset the links and flags (note eventDataControl!=event_data_control)
  fDevice->DeviceWrite(duneReg.eventDataControl, 0x00000000);
  // Registers in the Artix FPGA (DSP)
  // Release master logic reset & enable active channels

  fDevice->DeviceWrite(duneReg.master_logic_control, 0x00000041);
  
  fState=SSPDAQ::DeviceInterface::kRunning;
  fShouldStop=false;

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Starting read thread..."<<std::endl;
  fDataThread=new std::thread(&SSPDAQ::DeviceInterface::HardwareReadLoop,this);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Read thread is up!"<<std::endl;

  if(fRequestReceiver){
    fRequestReceiver->start();
  }

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Run started!"<<std::endl;
}

void SSPDAQ::DeviceInterface::HardwareReadLoop(){

  while(!fShouldStop){

    //    PrintHardwareState();

    /////////////////////////////////////////////////////////
    // Read an event from SSP. If there is no data, break. //
    /////////////////////////////////////////////////////////

    SSPDAQ::EventPacket newPacket;
    this->ReadEventFromDevice(newPacket);
    if(newPacket.header.header!=0xAAAAAAAA){
      usleep(1000);
      continue;
    }

    //newPacket.DumpHeader();
    //dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"HWRead getting mutex..."<<std::endl;
    std::unique_lock<std::mutex> mlock(fBufferMutex);
    //dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"HWRead got mutex!"<<std::endl;
    /////////////////////////////////////////////////////////
    // Push event onto deque.                              //
    /////////////////////////////////////////////////////////

    fPacketBuffer.emplace_back(std::move(newPacket));

    ///////////////////////////////////////////////////////////////
    // Pass event to trigger finder method. If it contains       //
    // a new trigger then add this to queue of pending triggers. //
    // If trigger start is before end of previous trigger then   //
    // fall over.                                                //
    ///////////////////////////////////////////////////////////////

    if(fRequestReceiver){
      while(true){
	auto t=fRequestReceiver->getNextRequest(0);
	if(t.timestamp==0){
	  break;
	}

        auto localTimestamp = t.timestamp*3 - fFragmentTimestampOffset;
	
	SSPDAQ::TriggerInfo newTrigger;
	
	newTrigger.triggerTime=localTimestamp;
	newTrigger.startTime=localTimestamp-fPreTrigLength;
	newTrigger.endTime=localTimestamp+fPostTrigLength;
	newTrigger.triggerType=0xFFFF;
	fTriggers.push(newTrigger);
      }
    }
    else{

      SSPDAQ::TriggerInfo newTrigger;
    
      if(this->GetTriggerInfo(fPacketBuffer.back(),newTrigger)){
	if(fTriggers.size()&&(newTrigger.startTime<fTriggers.back().endTime)){
	  dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Seen trigger with start time overlapping with previous, falling over!"<<std::endl;	
	  throw(EEventReadError());
	  //	set_exception(true);	
	  return;
	}
	fTriggers.push(newTrigger);
      }
    }

    ////////////////////////////////////////////////////////////
    //Cull old packets which are not associated with a trigger//
    ////////////////////////////////////////////////////////////

    unsigned long packetTime=GetTimestamp(fPacketBuffer.back().header);
    unsigned long firstInterestingTime;

    if(fTriggers.size()){
      firstInterestingTime=fTriggers.front().startTime-fTriggerWriteDelay;
    }
    else{
      firstInterestingTime=packetTime-fPreTrigLength-fTriggerLatency-fTriggerWriteDelay;
    }

    while(GetTimestamp(fPacketBuffer.front().header)<firstInterestingTime){
      fPacketBuffer.pop_front();
    }

    //dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"HWRead releasing mutex..."<<std::endl;
    mlock.unlock();
  }
  //  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"HWRead thread ending"<<std::endl;
}

void SSPDAQ::DeviceInterface::ReadEvents(std::vector<unsigned int>& fragment){

  if(fState!=kRunning){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to get data from non-running device refused!"<<std::endl;
    return;
  }

  /////////////////////////////////////////////////////////////////////////
  // Check whether current event timestamp is after end of next trigger. //
  // If so, pass trigger to fragment builder method.                     //
  /////////////////////////////////////////////////////////////////////////

  //  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"getNext thread getting mutex..."<<std::endl;
  std::unique_lock<std::mutex> mlock(fBufferMutex);

  if(!fTriggers.size()) return;
  //  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"getNext thread got mutex!"<<std::endl;
  unsigned long packetTime=GetTimestamp(fPacketBuffer.back().header);

  if(packetTime>fTriggers.front().endTime+fTriggerWriteDelay){
    this->BuildFragment(fTriggers.front(),fragment);
    fTriggers.pop();
  }
  //  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"getNext thread releasing mutex..."<<std::endl;
  mlock.unlock();
  
}


bool SSPDAQ::DeviceInterface::GetTriggerInfo(const SSPDAQ::EventPacket& event,SSPDAQ::TriggerInfo& newTrigger){

  static unsigned long currentTriggerTime=0;
  static bool channelsSeen[12]={false};

  static unsigned long lastDummyTrigger=0;

  int channel=event.header.group2&0x000F;
  unsigned long packetTime=GetTimestamp(event.header);

  if(fDummyPeriod>0&&(lastDummyTrigger!=packetTime/fDummyPeriod)){
    //    dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Generating dummy trigger for packet around "<<packetTime<<"!"<<std::endl;
    lastDummyTrigger=packetTime/fDummyPeriod;
    currentTriggerTime=packetTime;
    newTrigger.startTime=currentTriggerTime-fPreTrigLength;
    newTrigger.endTime=currentTriggerTime+fPostTrigLength;
    newTrigger.triggerType=event.header.group1&0xFFFF;
    
    for(unsigned int i=0;i<12;++i){
      channelsSeen[i]=false;
    }
    channelsSeen[channel]=true;
    return true; 
  }
    
  if(((event.header.group1&0xFFFF)&fTriggerMask)!=0){
    
    if(!channelsSeen[channel]&&packetTime<currentTriggerTime+1000){
      channelsSeen[channel]=true;
      //      dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Packet contains trigger word but this trigger was already generated from another channel"<<std::endl;
    }
    else{
      currentTriggerTime=packetTime;
      newTrigger.triggerTime=currentTriggerTime;
      newTrigger.startTime=currentTriggerTime-fPreTrigLength;
      newTrigger.endTime=currentTriggerTime+fPostTrigLength;
      newTrigger.triggerType=event.header.group1&0xFFFF;
      auto globalTimestamp = (packetTime + fFragmentTimestampOffset) / 3 ;
      dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Seen packet containing global trigger, timestamp "<<packetTime<<" / "<<globalTimestamp<<std::endl;
      for(unsigned int i=0;i<12;++i){
	channelsSeen[i]=false;
      }
      channelsSeen[channel]=true;
    return true;
    }
  }
  //dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Packet contains no trigger... trigger logic will ignore it."<<std::endl;
  return false;
}
  
void SSPDAQ::DeviceInterface::BuildFragment(const SSPDAQ::TriggerInfo& theTrigger,std::vector<unsigned int>& fragmentData){

  std::vector<SSPDAQ::EventPacket> eventsToPutBack;
  std::vector<SSPDAQ::EventPacket*> eventsToWrite;

  auto lastPacket=fPacketBuffer.begin();

  for(auto packetIter=fPacketBuffer.begin();packetIter!=fPacketBuffer.end();++packetIter){

    unsigned long packetTime=GetTimestamp(packetIter->header);

    if(packetTime>=theTrigger.startTime&&packetTime<theTrigger.endTime){
      
      lastPacket=packetIter;
    }
  }

  for(auto packetIter=fPacketBuffer.begin();;++packetIter){
    unsigned long packetTime=GetTimestamp(packetIter->header);
    
    if(packetTime>=theTrigger.startTime){
      if(packetTime>=theTrigger.endTime){
	eventsToPutBack.push_back(std::move(*packetIter));
      }
      else{
	eventsToWrite.push_back(&*packetIter);
      }
    }
    if(packetIter==lastPacket) break;
  }
    
  //=====================================//
  //Calculate required size of millislice//
  //=====================================//

  unsigned int dataSizeInWords=0;

  dataSizeInWords+=SSPDAQ::MillisliceHeader::sizeInUInts;
  for(auto ev=eventsToWrite.begin();ev!=eventsToWrite.end();++ev){
    dataSizeInWords+=(*ev)->header.length;
  }

  //==================//
  //Build slice header//
  //==================//

  SSPDAQ::MillisliceHeader sliceHeader;
  sliceHeader.length=dataSizeInWords;
  sliceHeader.nTriggers=eventsToWrite.size();
  sliceHeader.startTime=theTrigger.startTime;
  sliceHeader.endTime=theTrigger.endTime;
  sliceHeader.triggerTime=theTrigger.triggerTime;
  sliceHeader.triggerType=theTrigger.triggerType;

  //=================================================//
  //Allocate space for whole slice and fill with data//
  //=================================================//

  fragmentData.resize(dataSizeInWords);

  static unsigned int headerSizeInWords=
    sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);   //Size of DAQ event header

  //Put millislice header at front of vector
  auto sliceDataPtr=fragmentData.begin();
  unsigned int* millisliceHeaderPtr=(unsigned int*)((void*)(&sliceHeader));
  std::copy(millisliceHeaderPtr,millisliceHeaderPtr+SSPDAQ::MillisliceHeader::sizeInUInts,sliceDataPtr);

  //Fill rest of vector with event data
  sliceDataPtr+=SSPDAQ::MillisliceHeader::sizeInUInts;

  for(auto ev=eventsToWrite.begin();ev!=eventsToWrite.end();++ev){

    //DAQ event header
    unsigned int* headerPtr=(unsigned int*)((void*)(&((*ev)->header)));
    std::copy(headerPtr,headerPtr+headerSizeInWords,sliceDataPtr);
    
    //DAQ event payload
    sliceDataPtr+=headerSizeInWords;
    std::copy((*ev)->data.begin(),(*ev)->data.end(),sliceDataPtr);
    sliceDataPtr+=(*ev)->header.length-headerSizeInWords;
  }
  
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Building fragment with "<<eventsToWrite.size()<<" packets"<<std::endl;

  //=======================//
  //Add millislice to queue//
  //=======================//

  //This log message is too verbose...
  //dune::DAQLogger::LogDebug("SSP_DeviceInterface")<<this->GetIdentifier()<<"Pushing slice with "<<events.size()<<" triggers, starting at "<<startTime<<" onto queue!"<<std::endl;
  ++fMillislicesBuilt;

  unsigned int nDropped=0;

  if(lastPacket==fPacketBuffer.begin()){
    fPacketBuffer.pop_front();
    ++nDropped;
  }
  else{
    for(auto packetIter=(fPacketBuffer.begin())++;packetIter!=lastPacket;++packetIter){
      fPacketBuffer.pop_front();
      ++nDropped;
    }
    fPacketBuffer.pop_front();
    ++nDropped;
  }

  for(auto packetIter=eventsToPutBack.rbegin();packetIter!=eventsToPutBack.rend();++packetIter){
    fPacketBuffer.push_front(std::move(*packetIter));
  }
}

//void SSPDAQ::DeviceInterface::BuildEmptyMillislice(unsigned long startTime, unsigned long endTime){
//  std::vector<SSPDAQ::EventPacket> emptySlice;
//  this->BuildMillislice(emptySlice,startTime,endTime);
//}

/*void SSPDAQ::DeviceInterface::GetMillislice(std::vector<unsigned int>& sliceData){
  if(fQueue.try_pop(sliceData,std::chrono::microseconds(100000))){
    ++fMillislicesSent;//Try to pop from queue for 100ms
    if(!(fMillislicesSent%1000)){
    dune::DAQLogger::LogDebug("SSP_DeviceInterface")<<this->GetIdentifier()
		       <<"Interface sending slice "<<fMillislicesSent
		       <<", total built slices "<<fMillislicesBuilt
		       <<", current queue length "<<fQueue.size()<<std::endl;
    }
  }
  }*/

void SSPDAQ::DeviceInterface::ReadEventFromDevice(EventPacket& event){
  
  if(fState!=kRunning){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to get data from non-running device refused!"<<std::endl;
    event.SetEmpty();
    return;
  }

  std::vector<unsigned int> data;

  unsigned int skippedWords=0;
  unsigned int firstSkippedWord=0;

  unsigned int queueLengthInUInts=0;

  //Find first word in event header (0xAAAAAAAA)
  while(true){

    fDevice->DeviceQueueStatus(&queueLengthInUInts);
    data.clear();
    if(queueLengthInUInts){
      fDevice->DeviceReceive(data,1);
    }

    //If no data is available in pipe then return
    //without filling packet
    if(data.size()==0){
      if(skippedWords){
	dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipped "<<skippedWords<<"words and has not seen header for next event!"<<std::endl;
      }
      event.SetEmpty();
      return;
    }

    //Header found - continue reading rest of event
    if (data[0]==0xAAAAAAAA){
      break;
    }
    //Unexpected non-header word found - continue to
    //look for header word but need to issue warning
    if(data.size()>0){
      if(!skippedWords)firstSkippedWord=data[0];
      ++skippedWords;
      dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipping over word "<<data[0]<<" ("<<std::hex<<data[0]<<std::dec<<")"<<std::endl;
    }
  }

  if(skippedWords){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipped "<<skippedWords<<"words before finding next event header!"<<std::endl
			  <<"First skipped word was "<<std::hex<<firstSkippedWord<<std::dec<<std::endl;
  }
    
  unsigned int* headerBlock=(unsigned int*)&event.header;
  headerBlock[0]=0xAAAAAAAA;
  
  static const unsigned int headerReadSize=(sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)-1);

  //Wait for hardware queue to fill with full header data
  unsigned int timeWaited=0;//in us

  do{
    fDevice->DeviceQueueStatus(&queueLengthInUInts);
    if(queueLengthInUInts<headerReadSize){
      usleep(100); //100us
      //      dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: we slept after finding pattern word while waiting for header."<<std::endl;

      timeWaited+=100;
      if(timeWaited>10000000){ //10s
	try {
	  dune::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP delayed 10s between issuing header word and full header; giving up"
			    <<std::endl;
	} catch(...) {}
	event.SetEmpty();
	throw(EEventReadError());
      }
    }
  }while(queueLengthInUInts<headerReadSize);

  //Get header from device and check it is the right length
  fDevice->DeviceReceive(data,headerReadSize);
  if(data.size()!=headerReadSize){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP returned truncated header even though FIFO queue is of sufficient length!"
			<<std::endl;
    } catch (...) {}
    event.SetEmpty();
    throw(EEventReadError());
  }

  //Copy header into event packet
  std::copy(data.begin(),data.end(),&(headerBlock[1]));

  //Wait for hardware queue to fill with full event data
  unsigned int bodyReadSize=event.header.length-(sizeof(EventHeader)/sizeof(unsigned int));
  queueLengthInUInts=0;
  timeWaited=0;//in us

  do{
    fDevice->DeviceQueueStatus(&queueLengthInUInts);
    if(queueLengthInUInts<bodyReadSize){
      usleep(100); //100us
      //      dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: we slept after finding header before reading full event."<<std::endl;
      timeWaited+=100;
      if(timeWaited>10000000){ //10s
	try {
	dune::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP delayed 10s between issuing header and full event; giving up"
			    <<std::endl;
	} catch(...) {}
	event.DumpHeader();
	event.SetEmpty();
	throw(EEventReadError());
      }
    }
  }while(queueLengthInUInts<bodyReadSize);
   
  //Get event from SSP and check that it is the right length
  fDevice->DeviceReceive(data,bodyReadSize);

  if(data.size()!=bodyReadSize){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP returned truncated event even though FIFO queue is of sufficient length!"
			<<std::endl;
    } catch (...) {}
    event.SetEmpty();
    throw(EEventReadError());
  }

  //Copy event data into event packet
  event.data=std::move(data);
  //event.DumpHeader();
  return;
}

void SSPDAQ::DeviceInterface::Shutdown(){
  fDevice->Close();
  fState=kUninitialized;
}

void SSPDAQ::DeviceInterface::SetRegister(unsigned int address, unsigned int value,
					  unsigned int mask){

  if(mask==0xFFFFFFFF){
    fDevice->DeviceWrite(address,value);
  }
  else{
    fDevice->DeviceWriteMask(address,mask,value);
  }
}

void SSPDAQ::DeviceInterface::SetRegisterArray(unsigned int address, std::vector<unsigned int> value){

    this->SetRegisterArray(address,&(value[0]),value.size());
}

void SSPDAQ::DeviceInterface::SetRegisterArray(unsigned int address, unsigned int* value, unsigned int size){

  fDevice->DeviceArrayWrite(address,size,value);
}

void SSPDAQ::DeviceInterface::ReadRegister(unsigned int address, unsigned int& value,
					  unsigned int mask){

  if(mask==0xFFFFFFFF){
    fDevice->DeviceRead(address,&value);
  }
  else{
    fDevice->DeviceReadMask(address,mask,&value);
  }
}

void SSPDAQ::DeviceInterface::ReadRegisterArray(unsigned int address, std::vector<unsigned int>& value, unsigned int size){

  value.resize(size);
  this->ReadRegisterArray(address,&(value[0]),size);
}

void SSPDAQ::DeviceInterface::ReadRegisterArray(unsigned int address, unsigned int* value, unsigned int size){

  fDevice->DeviceArrayRead(address,size,value);
}
void SSPDAQ::DeviceInterface::SetRegisterByName(std::string name, unsigned int value){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name];
  
  this->SetRegister(reg,value,reg.WriteMask());
}

void SSPDAQ::DeviceInterface::SetRegisterElementByName(std::string name, unsigned int index, unsigned int value){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name][index];
  
  this->SetRegister(reg,value,reg.WriteMask());
}

void SSPDAQ::DeviceInterface::SetRegisterArrayByName(std::string name, unsigned int value){
  unsigned int regSize=(SSPDAQ::RegMap::Get())[name].Size();
  std::vector<unsigned int> arrayContents(regSize,value);

  this->SetRegisterArrayByName(name,arrayContents);
}

void SSPDAQ::DeviceInterface::SetRegisterArrayByName(std::string name, std::vector<unsigned int> values){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name];
  if(reg.Size()!=values.size()){
    try {
      dune::DAQLogger::LogError("SSP_DeviceInterface")<<"Request to set named register array "<<name<<", length "<<reg.Size()
  			<<"with vector of "<<values.size()<<" values!"<<std::endl;
    } catch (...) {}
    throw(std::invalid_argument(""));
  }
  this->SetRegisterArray(reg[0],values);
}

void SSPDAQ::DeviceInterface::ReadRegisterByName(std::string name, unsigned int& value){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name];
  
  this->ReadRegister(reg,value,reg.ReadMask());
}

void SSPDAQ::DeviceInterface::ReadRegisterElementByName(std::string name, unsigned int index, unsigned int& value){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name][index];
  
  this->ReadRegister(reg,value,reg.ReadMask());
}

void SSPDAQ::DeviceInterface::ReadRegisterArrayByName(std::string name, std::vector<unsigned int>& values){
  SSPDAQ::RegMap::Register reg=(SSPDAQ::RegMap::Get())[name];
  this->ReadRegisterArray(reg[0],values,reg.Size());
}



void SSPDAQ::DeviceInterface::Configure(){

  if(fState!=kStopped){
    dune::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to reconfigure non-stopped device refused!"<<std::endl;
    return;
  }

  SSPDAQ::RegMap& duneReg = SSPDAQ::RegMap::Get();

  	// Setting up some constants to use during initialization
	const uint	module_id		= 0xABC;	// This value is reported in the event header
	const uint	channel_control[12] = 
	//	Channel Control Bit Descriptions
	//	31		cfd_enable
	//	30		pileup_waveform_only
	//	26		pileup_extend_enable
	//	25:24	event_extend_mode
	//	23		disc_counter_mode
	//	22		ahit_counter_mode 
	//	21		ACCEPTED_EVENT_COUNTER_MODE
	//	20		dropped_event_counter_mode
	//	15:14	external_disc_flag_sel
	//	13:12	external_disc_mode
	//	11		negative edge trigger enable
	//	10		positive edge trigger enable
	//	6:4		This sets the timestamp trigger rate (source of external_disc_flag_in(3) into channel logic)
	//	2		Not pileup_disable
	//	1		Trigger Mode
	//	0		channel enable
	//
	//	Channel Control Examples
	//	0x00000000,		// disable channel #
	//	0x80F00001,		// enable channel # but do not enable any triggers (CFD Enabled)
	//	0x80F00401,		// configure channel # in positive self-trigger mode (CFD Enabled)
	//	0x80F00801,		// configure channel # in negative self-trigger mode (CFD Enabled)
	//	0x80F00C01,		// configure channel # in positive and negative self-trigger mode (CFD Enabled)
	//	0x00F06001,		// configure channel # in external trigger mode.
	//	0x00F0E051,		// configure channel # in a slow timestamp triggered mode (8.941Hz)
	//	0x00F0E061,		// configure channel # in a very slow timestamp triggered mode (1.118Hz)
	{
		0x00F0E0C1,		// configure channel #0 in a slow timestamp triggered mode
		0x00000000,		// disable channel #1
		0x00000000,		// disable channel #2
		0x00000000,		// disable channel #3
		0x00000000,		// disable channel #4
		0x00000000,		// disable channel #5
		0x00000000,		// disable channel #6
		0x00000000,		// disable channel #7
		0x00000000,		// disable channel #8
		0x00000000,		// disable channel #9
		0x00000000,		// disable channel #10
		0x00000000,		// disable channel #11
	};
	const uint	led_threshold		= 25;	
	const uint	cfd_fraction		= 0x1800;
	const uint	readout_pretrigger	= 100;	
	const uint	event_packet_length	= 2046;	
	const uint	p_window			= 0;	
	const uint	i2_window			= 500;
	const uint	m1_window			= 10;	
	const uint	m2_window			= 10;	
	const uint	d_window			= 20;
	const uint  i1_window			= 500;
	const uint	disc_width			= 10;
	const uint	baseline_start		= 0x0000;
	const uint	baseline_delay		= 5;

	int i = 0;
	uint data[12];

	// This script of register writes sets up the digitizer for basic real event operation
	// Comments next to each register are excerpts from the VHDL or C code
	// ALL existing registers are shown here however many are commented out because they are
	// status registers or simply don't need to be modified
	// The script runs through the registers numerically (increasing addresses)
	// Therefore, it is assumed DeviceStopReset() has been called so these changes will not
	// cause crazy things to happen along the way

	fDevice->DeviceWrite(duneReg.c2c_control,0x00000007);
	fDevice->DeviceWrite(duneReg.c2c_master_intr_control,0x00000000);
	fDevice->DeviceWrite(duneReg.comm_clock_control,0x00000001);
	fDevice->DeviceWrite(duneReg.comm_led_config, 0x00000000);
	fDevice->DeviceWrite(duneReg.comm_led_input, 0x00000000);
	fDevice->DeviceWrite(duneReg.qi_dac_config,0x00000000);
	fDevice->DeviceWrite(duneReg.qi_dac_control,0x00000001);

	fDevice->DeviceWrite(duneReg.bias_config[0],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[1],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[2],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[3],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[4],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[5],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[6],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[7],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[8],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[9],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[10],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_config[11],0x00000000);
	fDevice->DeviceWrite(duneReg.bias_control,0x00000001);

	fDevice->DeviceWrite(duneReg.vmon_config,0x0012F000);
	fDevice->DeviceWrite(duneReg.vmon_select,0x00FFFF00);
	fDevice->DeviceWrite(duneReg.vmon_gpio,0x00000000);
	fDevice->DeviceWrite(duneReg.vmon_control,0x00010001);
	fDevice->DeviceWrite(duneReg.imon_config,0x0012F000);
	fDevice->DeviceWrite(duneReg.imon_select,0x00FFFF00);
	fDevice->DeviceWrite(duneReg.imon_gpio,0x00000000);
	fDevice->DeviceWrite(duneReg.imon_control,0x00010001);

	//Registers in the Artix FPGA (DSP)//AddressDefault ValueRead MaskWrite MaskCode Name
	fDevice->DeviceWrite(duneReg.module_id,module_id);
	fDevice->DeviceWrite(duneReg.c2c_slave_intr_control,0x00000000);

	for (i = 0; i < 12; i++) data[i] = channel_control[i];
	fDevice->DeviceArrayWrite(duneReg.channel_control[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = led_threshold;
	fDevice->DeviceArrayWrite(duneReg.led_threshold[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = cfd_fraction;
	fDevice->DeviceArrayWrite(duneReg.cfd_parameters[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = readout_pretrigger;
	fDevice->DeviceArrayWrite(duneReg.readout_pretrigger[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = event_packet_length;
	fDevice->DeviceArrayWrite(duneReg.readout_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = p_window;
	fDevice->DeviceArrayWrite(duneReg.p_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = i2_window;
	fDevice->DeviceArrayWrite(duneReg.i2_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = m1_window;
	fDevice->DeviceArrayWrite(duneReg.m1_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = m2_window;
	fDevice->DeviceArrayWrite(duneReg.m2_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = d_window;
	fDevice->DeviceArrayWrite(duneReg.d_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = i1_window;
	fDevice->DeviceArrayWrite(duneReg.i1_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = disc_width;
	fDevice->DeviceArrayWrite(duneReg.disc_width[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = baseline_start;
	fDevice->DeviceArrayWrite(duneReg.baseline_start[0], 12, data);

	fDevice->DeviceWrite(duneReg.trigger_input_delay,0x00000001);
	fDevice->DeviceWrite(duneReg.gpio_output_width,0x00001000);
	fDevice->DeviceWrite(duneReg.front_panel_config, 0x00001111);
	fDevice->DeviceWrite(duneReg.dsp_led_config,0x00000000);
	fDevice->DeviceWrite(duneReg.dsp_led_input, 0x00000000);
	fDevice->DeviceWrite(duneReg.baseline_delay,baseline_delay);
	fDevice->DeviceWrite(duneReg.diag_channel_input,0x00000000);
	fDevice->DeviceWrite(duneReg.qi_config,0x0FFF1F00);
	fDevice->DeviceWrite(duneReg.qi_delay,0x00000000);
	fDevice->DeviceWrite(duneReg.qi_pulse_width,0x00000000);
	fDevice->DeviceWrite(duneReg.external_gate_width,0x00008000);
	//fDevice->DeviceWrite(duneReg.dsp_clock_control,0x00000000);

	// Load the window settings - This MUST be the last operation

}

std::string SSPDAQ::DeviceInterface::GetIdentifier(){

  std::string ident;
  ident+="SSP@";
  if(fCommType==SSPDAQ::kUSB){
    ident+="(USB";
    ident+=fDeviceId;
    ident+="):";
  }
  else if(fCommType==SSPDAQ::kEthernet){
    boost::asio::ip::address ip=boost::asio::ip::address_v4(fDeviceId);
    std::string ipString=ip.to_string();
    ident+="(";
    ident+=ipString;
    ident+="):";
  }
  else if(fCommType==SSPDAQ::kEmulated){
    ident+="(EMULATED";
    ident+=fDeviceId;
    ident+="):";
  }
  return ident;
}

unsigned long SSPDAQ::DeviceInterface::GetTimestamp(const SSPDAQ::EventHeader& header){
  unsigned long packetTime=0;
  if(fUseExternalTimestamp){
    for(unsigned int iWord=0;iWord<=3;++iWord){
      packetTime+=((unsigned long)(header.timestamp[iWord]))<<16*iWord;
    }
  }
  else{
    for(unsigned int iWord=1;iWord<=3;++iWord){
      packetTime+=((unsigned long)(header.intTimestamp[iWord]))<<16*(iWord-1);
    }
  }    
  return packetTime;
}

void SSPDAQ::DeviceInterface::PrintHardwareState(){

  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"===SSP DIAGNOSTIC REGISTERS==="<<std::endl;

  SSPDAQ::RegMap& duneReg=SSPDAQ::RegMap::Get();
  unsigned int val;

  fDevice->DeviceRead(duneReg.dp_clock_status, &val);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"dsp_clock_status: "<<std::hex<<val<<std::endl;
  fDevice->DeviceRead(duneReg.live_timestamp_msb, &val);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"live_timestamp_msb: "<<val<<std::endl;
  fDevice->DeviceRead(duneReg.live_timestamp_lsb, &val);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"live_timestamp_lsb: "<<val<<std::endl;
  fDevice->DeviceRead(duneReg.sync_delay, &val);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"sync_delay: "<<val<<std::endl;
  fDevice->DeviceRead(duneReg.sync_count, &val);
  dune::DAQLogger::LogInfo("SSP_DeviceInterface")<<"sync_count: "<<val<<std::dec<<std::endl;
  
}
