#include "DeviceInterface.h"
#include "anlExceptions.h"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"
#include "RegMap.h"
#include <time.h>
#include <utility>
#include "boost/asio.hpp"

SSPDAQ::DeviceInterface::DeviceInterface(SSPDAQ::Comm_t commType, unsigned long deviceId)
  : fCommType(commType), fDeviceId(deviceId), fState(SSPDAQ::DeviceInterface::kUninitialized),
    fMillisliceLength(1E8), fMillisliceOverlap(1E7), fUseExternalTimestamp(false),
    fHardwareClockRateInMHz(128), fEmptyWriteDelayInus(100000000), fSlowControlOnly(false),
    fStartOnNOvASync(true){
  fReadThread=0;
}

void SSPDAQ::DeviceInterface::OpenSlowControl(){
  //Ask device manager for a pointer to the specified device
  SSPDAQ::DeviceManager& devman=SSPDAQ::DeviceManager::Get();

  SSPDAQ::Device* device=0;

  lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Opening "<<((fCommType==SSPDAQ::kUSB)?"USB":((fCommType==SSPDAQ::kEthernet)?"Ethernet":"Emulated"))
		     <<" device #"<<fDeviceId<<" for slow control only..."<<std::endl;
  
  device=devman.OpenDevice(fCommType,fDeviceId,true);
  
  if(!device){
    try {
      lbne::DAQLogger::LogError("SSP_DeviceInterface")<<"Unable to get handle to device; giving up!"<<std::endl;
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

  lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Initializing "<<((fCommType==SSPDAQ::kUSB)?"USB":((fCommType==SSPDAQ::kEthernet)?"Ethernet":"Emulated"))
		     <<" device #"<<fDeviceId<<"..."<<std::endl;
  
  device=devman.OpenDevice(fCommType,fDeviceId);
  
  if(!device){
    try {
      lbne::DAQLogger::LogError("SSP_DeviceInterface")<<"Unable to get handle to device; giving up!"<<std::endl;
    } catch (...) {}
      throw(ENoSuchDevice());
  }

  fDevice=device;

  //Put device into sensible state
  this->Stop();
}

void SSPDAQ::DeviceInterface::Stop(){
  if(fState!=SSPDAQ::DeviceInterface::kRunning&&
     fState!=SSPDAQ::DeviceInterface::kUninitialized){
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Running stop command for non-running device!"<<std::endl;
  }

  if(fState==SSPDAQ::DeviceInterface::kRunning){
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Device interface stopping run"<<std::endl;
  }

  SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
      
  fShouldStop=true;

  if(fReadThread){
    fReadThread->join();
    fReadThread.reset();
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Read thread terminated"<<std::endl;
  }  

  fDevice->DeviceWrite(lbneReg.eventDataControl, 0x0013001F);
  fDevice->DeviceClear(lbneReg.master_logic_control, 0x00000101);
  // Clear the FIFOs
  fDevice->DeviceWrite(lbneReg.fifo_control, 0x08000000);
  fDevice->DeviceWrite(lbneReg.PurgeDDR, 0x00000001);
  // Reset the links and flags				
  fDevice->DeviceWrite(lbneReg.event_data_control, 0x00020001);
  // Flush RX buffer
  fDevice->DevicePurgeData();
  lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Hardware set to stopped state"<<std::endl;
  fState=SSPDAQ::DeviceInterface::kStopped;

  if(fState==SSPDAQ::DeviceInterface::kRunning){
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"DeviceInterface stop transition complete!"<<std::endl;
  }
}


void SSPDAQ::DeviceInterface::Start(bool startReadThread){

  if(fState!=kStopped){
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to start acquisition on non-stopped device refused!"<<std::endl;
    return;
  }
 
  if(fSlowControlOnly){
    lbne::DAQLogger::LogError("SSP_DeviceInterface")<<"Naughty Zoot! Attempt to start run on slow control interface refused!"<<std::endl;
    return;
  }

  lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"Device interface starting run"<<std::endl;
  SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
  // This script enables all logic and FIFOs and starts data acquisition in the device
  // Operations MUST be performed in this order
  
  //Load window settings and bias voltage into channels
  fDevice->DeviceWrite(lbneReg.channel_pulsed_control, 0x1);
  fDevice->DeviceWrite(lbneReg.bias_control, 0x1);
  fDevice->DeviceWriteMask(lbneReg.mon_control, 0x1, 0x1);

  fDevice->DeviceWrite(lbneReg.event_data_control, 0x00000000);
  // Release the FIFO reset						
  fDevice->DeviceWrite(lbneReg.fifo_control, 0x00000000);
  // Registers in the Zynq FPGA (Comm)
  // Reset the links and flags (note eventDataControl!=event_data_control)
  fDevice->DeviceWrite(lbneReg.eventDataControl, 0x00000000);
  // Registers in the Artix FPGA (DSP)
  // Release master logic reset & enable active channels

  unsigned long runStartTime=0;
  if(fStartOnNOvASync){
    fDevice->DeviceWrite(lbneReg.master_logic_control, 0x00000100);
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<GetIdentifier()<<"Hardware waiting for NOvA sync to start run"<<std::endl;
  }
  else{
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<GetIdentifier()<<"Starting run without NOvA sync!"<<std::endl;
    fDevice->DeviceWrite(lbneReg.master_logic_control, 0x00000001);
  }


  fShouldStop=false;
  fState=SSPDAQ::DeviceInterface::kRunning;
  lbne::DAQLogger::LogDebug("SSP_DeviceInterface")<<"Device interface starting read thread...";

  //In normal operation DeviceInterface will start read thread and form millislices.
  //If user will manually call ReadEventFromDevice to get events, this can be disabled.
  if(startReadThread){
    fReadThread=std::unique_ptr<std::thread>(new std::thread(&SSPDAQ::DeviceInterface::ReadEvents,this,runStartTime));
  }

  lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<"DAQ run started!"<<std::endl;
}

void SSPDAQ::DeviceInterface::ReadEvents(unsigned long runStartTime){

  if(fState!=kRunning){
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to get data from non-running device refused!"<<std::endl;
    return;
  }

  //Holding pattern while waiting for hardware to start
  if(fStartOnNOvASync){
    SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
    unsigned int masterLogicStatus=0;
    while(!masterLogicStatus){
      usleep(10000);//10ms
      fDevice->DeviceRead(lbneReg.master_logic_status,&masterLogicStatus);
    }
    unsigned int timestampBuf;
    fDevice->DeviceRead(lbneReg.sync_stamp_low,&timestampBuf);
    runStartTime=timestampBuf;
    fDevice->DeviceRead(lbneReg.sync_stamp_high,&timestampBuf);
    runStartTime+=(static_cast<unsigned long>(timestampBuf))<<32;
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<GetIdentifier()<<"Hardware synced at "<<runStartTime<<", starting run"<<std::endl;
  }

  bool useExternalTimestamp=fUseExternalTimestamp;
  bool hasSeenEvent=false;
  bool hasFinishedFirstSlice=false;
  unsigned int discardedEvents=0;
  unsigned long millisliceStartTime=runStartTime;
  unsigned long millisliceLengthInTicks=fMillisliceLength;//0.67s
  unsigned long millisliceOverlapInTicks=fMillisliceOverlap;//0.067s
  unsigned long sleepTime=0;
  unsigned int millisliceCount=0;

  bool haveWarnedNoEvents=false;

  //Need three lots of event packets, to manage overlap between slices
  //and potential non-strict time ordering of events from hardware
  std::vector<SSPDAQ::EventPacket> events_prevSlice;
  std::vector<SSPDAQ::EventPacket> events_thisSlice;
  std::vector<SSPDAQ::EventPacket> events_nextSlice;

  fMillislicesSent=0;
  fMillislicesBuilt=0;

  //Check whether other thread has set the stop flag
  //Really need to know the timestamp at which to stop so we build the
  //same total number of slices as the other generators
  while(!fShouldStop){
    //Ask for event and check that one was returned.
    //ReadEventFromDevice Will return an empty packet with header word set to 0xDEADBEEF
    //if there was no event to read from the SSP.
    SSPDAQ::EventPacket event;
    this->ReadEventFromDevice(event);

    //If there is no event, sleep for a bit and try again
    if(event.header.header!=0xAAAAAAAA){
      usleep(1000);//1ms
      sleepTime+=1000;

      //If we are waiting a long time, assume that there are no events in this period,
      //and fill a millislice anyway.
      //This stops the aggregator waiting indefinitely for a fragment.
      if(sleepTime>fEmptyWriteDelayInus&&(hasSeenEvent||runStartTime!=0)){	
	if(!haveWarnedNoEvents){
	  lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: DeviceInterface is seeing no events, starting to write empty slices"<<std::endl;
	  haveWarnedNoEvents=true;
	}

	//Write prevSlice and swap slices back through containers
	if(hasFinishedFirstSlice){
	  this->BuildMillislice(events_prevSlice,millisliceStartTime-millisliceLengthInTicks,millisliceStartTime+millisliceOverlapInTicks);
	}
	else{
	  hasFinishedFirstSlice=true;
	}
	events_prevSlice.clear();
	std::swap(events_prevSlice,events_thisSlice);
	std::swap(events_thisSlice,events_nextSlice);
	++millisliceCount;
	millisliceStartTime+=millisliceLengthInTicks;
	sleepTime-=1./fHardwareClockRateInMHz*fMillisliceLength+10;
      }
      continue;
    }
    sleepTime=0;
    haveWarnedNoEvents=false;

    //Have an event! Get the timestamp and decide what to do with the event
    
    //Convert unsigned shorts into 1*unsigned long event timestamp
    unsigned long eventTime=0;
    if(useExternalTimestamp){
      for(unsigned int iWord=0;iWord<=3;++iWord){
	eventTime+=((unsigned long)(event.header.timestamp[iWord]))<<16*iWord;
      }
    }
    else{
      for(unsigned int iWord=1;iWord<=3;++iWord){
	eventTime+=((unsigned long)(event.header.intTimestamp[iWord]))<<16*(iWord-1);
      }
    }

    //Deal with stuff for first event
    if(!hasSeenEvent){
      //If there is no run start time set, just start at time of first event
      if(runStartTime==0){
	runStartTime=eventTime;
	millisliceStartTime=runStartTime;
	hasSeenEvent=true;
      }
      //Otherwise discard events which happen before run start time
      else{
	if(eventTime<runStartTime){
	  ++discardedEvents;
	  continue;
	}
	else{
	  hasSeenEvent=true;
	}
      }
    }

    lbne::DAQLogger::LogDebug("SSP_DeviceInterface")<<this->GetIdentifier()<<"Interface got event with timestamp "<<eventTime<<"("<<(eventTime-runStartTime)/150E6<<"s from run start)"<<std::endl;

    bool haveWrittenEvent=false;

    //Continue building slices until we reach the one(s) containing the event
    while(!haveWrittenEvent){

      //Have already written and cleared slice containing the event. Very bad!
      if(eventTime<millisliceStartTime-millisliceLengthInTicks){
	if(events_thisSlice.size()){
	  events_thisSlice.back().DumpEvent();
	}
	else if(events_prevSlice.size()){
	  events_prevSlice.back().DumpEvent();
	}
	try {
	  lbne::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"Error: Event seen with timestamp less than start of first available slice ("
			    <<eventTime<<" vs "<<millisliceStartTime-millisliceLengthInTicks<<")!"<<std::endl;
	} catch (...) {}

	event.DumpEvent();
	throw(EEventReadError("Bad timestamp"));
      }
      //Event is in previous slice (due to events arriving out of order from hardware)
      else if(eventTime<millisliceStartTime){
	events_prevSlice.push_back(std::move(event));
	haveWrittenEvent=true;
      }
      //Event is in current slice and overlap window of previous slice
      else if(eventTime<millisliceStartTime+millisliceOverlapInTicks){
	events_prevSlice.push_back(std::move(event));
	events_thisSlice.push_back(events_prevSlice.back());
	haveWrittenEvent=true;
      }
      //Event only in the current slice
      else if(eventTime<millisliceStartTime+millisliceLengthInTicks){
	events_thisSlice.push_back(std::move(event));
	haveWrittenEvent=true;
      }
      //Event is in next slice, but in the overlap window of the current slice
      else if(eventTime<millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks){
	events_thisSlice.push_back(std::move(event));
	events_nextSlice.push_back(events_thisSlice.back());
	haveWrittenEvent=true;
      }
      //Event is later than overlap window of current slice
      //In this case, we will write a slice.
      //The while loop will keep getting to this condition
      //until the slice containing the event is found.
      else{
	lbne::DAQLogger::LogDebug("SSP_DeviceInterface")<<this->GetIdentifier()<<"Device interface building millislice with "<<events_prevSlice.size()<<" events"<<std::endl;
	//Write prevSlice and swap slices back through containers
	if(hasFinishedFirstSlice){
	  this->BuildMillislice(events_prevSlice,millisliceStartTime-millisliceLengthInTicks,millisliceStartTime+millisliceOverlapInTicks);
	}
	else{
	  hasFinishedFirstSlice=true;
	}
	events_prevSlice.clear();
	std::swap(events_prevSlice,events_thisSlice);
	std::swap(events_thisSlice,events_nextSlice);
	++millisliceCount;
	millisliceStartTime+=millisliceLengthInTicks;	
      }
    }
  }
}
  
void SSPDAQ::DeviceInterface::BuildMillislice(const std::vector<EventPacket>& events,unsigned long startTime,unsigned long endTime){

  //=====================================//
  //Calculate required size of millislice//
  //=====================================//

  unsigned int dataSizeInWords=0;

  dataSizeInWords+=SSPDAQ::MillisliceHeader::sizeInUInts;
  for(auto ev=events.begin();ev!=events.end();++ev){
    dataSizeInWords+=ev->header.length;
  }

  //==================//
  //Build slice header//
  //==================//

  SSPDAQ::MillisliceHeader sliceHeader;
  sliceHeader.length=dataSizeInWords;
  sliceHeader.nTriggers=events.size();
  sliceHeader.startTime=startTime;
  sliceHeader.endTime=endTime;

  //=================================================//
  //Allocate space for whole slice and fill with data//
  //=================================================//

  std::vector<unsigned int> sliceData(dataSizeInWords);

  static unsigned int headerSizeInWords=
    sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);   //Size of DAQ event header

  //Put millislice header at front of vector
  auto sliceDataPtr=sliceData.begin();
  unsigned int* millisliceHeaderPtr=(unsigned int*)((void*)(&sliceHeader));
  std::copy(millisliceHeaderPtr,millisliceHeaderPtr+SSPDAQ::MillisliceHeader::sizeInUInts,sliceDataPtr);

  //Fill rest of vector with event data
  sliceDataPtr+=SSPDAQ::MillisliceHeader::sizeInUInts;

  for(auto ev=events.begin();ev!=events.end();++ev){

    //DAQ event header
    unsigned int* headerPtr=(unsigned int*)((void*)(&ev->header));
    std::copy(headerPtr,headerPtr+headerSizeInWords,sliceDataPtr);
    
    //DAQ event payload
    sliceDataPtr+=headerSizeInWords;
    std::copy(ev->data.begin(),ev->data.end(),sliceDataPtr);
    sliceDataPtr+=ev->header.length-headerSizeInWords;
  }
  
  //=======================//
  //Add millislice to queue//
  //=======================//

  lbne::DAQLogger::LogDebug("SSP_DeviceInterface")<<this->GetIdentifier()<<"Pushing slice with "<<events.size()<<" triggers, starting at "<<startTime<<" onto queue!"<<std::endl;
  fQueue.push(std::move(sliceData));

  ++fMillislicesBuilt;

}

void SSPDAQ::DeviceInterface::BuildEmptyMillislice(unsigned long startTime, unsigned long endTime){
  std::vector<SSPDAQ::EventPacket> emptySlice;
  this->BuildMillislice(emptySlice,startTime,endTime);
}

void SSPDAQ::DeviceInterface::GetMillislice(std::vector<unsigned int>& sliceData){
  if(fQueue.try_pop(sliceData,std::chrono::microseconds(100000))){
    ++fMillislicesSent;//Try to pop from queue for 100ms
    if(!(fMillislicesSent%1000)){
    lbne::DAQLogger::LogInfo("SSP_DeviceInterface")<<this->GetIdentifier()
		       <<"Interface sending slice "<<fMillislicesSent
		       <<", total built slices "<<fMillislicesBuilt
		       <<", current queue length "<<fQueue.size()<<std::endl;
    }
  }
}

void SSPDAQ::DeviceInterface::ReadEventFromDevice(EventPacket& event){
  
  if(fState!=kRunning){
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to get data from non-running device refused!"<<std::endl;
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
	lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipped "<<skippedWords<<"words and has not seen header for next event!"<<std::endl;
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
      lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipping over word "<<data[0]<<" ("<<std::hex<<data[0]<<std::dec<<")"<<std::endl;
    }
  }

  if(skippedWords){
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<this->GetIdentifier()<<"Warning: GetEvent skipped "<<skippedWords<<"words before finding next event header!"<<std::endl
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
      usleep(1000); //1ms
      timeWaited+=1000;
      if(timeWaited>10000000){ //10s

	try {
	lbne::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP delayed 10s between issuing header word and full header; giving up"
			    <<std::endl;
	} catch (...) {}
	event.SetEmpty();
	throw(EEventReadError());
      }
    }
  }while(queueLengthInUInts<headerReadSize);

  //Get header from device and check it is the right length
  fDevice->DeviceReceive(data,headerReadSize);
  if(data.size()!=headerReadSize){
    try {
    lbne::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP returned truncated header even though FIFO queue is of sufficient length!"
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
      usleep(1000); //1ms
      timeWaited+=1000;
      if(timeWaited>10000000){ //10s
	try {
	lbne::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP delayed 10s between issuing header and full event; giving up"
			    <<std::endl;
	} catch (...) {}
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
      lbne::DAQLogger::LogError("SSP_DeviceInterface")<<this->GetIdentifier()<<"SSP returned truncated event even though FIFO queue is of sufficient length!"
			<<std::endl;
    } catch (...) {}

    event.SetEmpty();
    throw(EEventReadError());
  }

  //Copy event data into event packet
  event.data=std::move(data);

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
      lbne::DAQLogger::LogError("SSP_DeviceInterface")<<"Request to set named register array "<<name<<", length "<<reg.Size()
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
    lbne::DAQLogger::LogWarning("SSP_DeviceInterface")<<"Attempt to reconfigure non-stopped device refused!"<<std::endl;
    return;
  }

  SSPDAQ::RegMap& lbneReg = SSPDAQ::RegMap::Get();

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

	fDevice->DeviceWrite(lbneReg.c2c_control,0x00000007);
	fDevice->DeviceWrite(lbneReg.c2c_master_intr_control,0x00000000);
	fDevice->DeviceWrite(lbneReg.comm_clock_control,0x00000001);
	fDevice->DeviceWrite(lbneReg.comm_led_config, 0x00000000);
	fDevice->DeviceWrite(lbneReg.comm_led_input, 0x00000000);
	fDevice->DeviceWrite(lbneReg.qi_dac_config,0x00000000);
	fDevice->DeviceWrite(lbneReg.qi_dac_control,0x00000001);

	fDevice->DeviceWrite(lbneReg.bias_config[0],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[1],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[2],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[3],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[4],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[5],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[6],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[7],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[8],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[9],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[10],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_config[11],0x00000000);
	fDevice->DeviceWrite(lbneReg.bias_control,0x00000001);

	fDevice->DeviceWrite(lbneReg.mon_config,0x0012F000);
	fDevice->DeviceWrite(lbneReg.mon_select,0x00FFFF00);
	fDevice->DeviceWrite(lbneReg.mon_gpio,0x00000000);
	fDevice->DeviceWrite(lbneReg.mon_control,0x00010001);

	//Registers in the Artix FPGA (DSP)//AddressDefault ValueRead MaskWrite MaskCode Name
	fDevice->DeviceWrite(lbneReg.module_id,module_id);
	fDevice->DeviceWrite(lbneReg.c2c_slave_intr_control,0x00000000);

	for (i = 0; i < 12; i++) data[i] = channel_control[i];
	fDevice->DeviceArrayWrite(lbneReg.channel_control[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = led_threshold;
	fDevice->DeviceArrayWrite(lbneReg.led_threshold[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = cfd_fraction;
	fDevice->DeviceArrayWrite(lbneReg.cfd_parameters[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = readout_pretrigger;
	fDevice->DeviceArrayWrite(lbneReg.readout_pretrigger[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = event_packet_length;
	fDevice->DeviceArrayWrite(lbneReg.readout_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = p_window;
	fDevice->DeviceArrayWrite(lbneReg.p_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = i2_window;
	fDevice->DeviceArrayWrite(lbneReg.i2_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = m1_window;
	fDevice->DeviceArrayWrite(lbneReg.m1_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = m2_window;
	fDevice->DeviceArrayWrite(lbneReg.m2_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = d_window;
	fDevice->DeviceArrayWrite(lbneReg.d_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = i1_window;
	fDevice->DeviceArrayWrite(lbneReg.i1_window[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = disc_width;
	fDevice->DeviceArrayWrite(lbneReg.disc_width[0], 12, data);

	for (i = 0; i < 12; i++) data[i] = baseline_start;
	fDevice->DeviceArrayWrite(lbneReg.baseline_start[0], 12, data);

	fDevice->DeviceWrite(lbneReg.trigger_input_delay,0x00000001);
	fDevice->DeviceWrite(lbneReg.gpio_output_width,0x00001000);
	fDevice->DeviceWrite(lbneReg.front_panel_config, 0x00001111);
	fDevice->DeviceWrite(lbneReg.dsp_led_config,0x00000000);
	fDevice->DeviceWrite(lbneReg.dsp_led_input, 0x00000000);
	fDevice->DeviceWrite(lbneReg.baseline_delay,baseline_delay);
	fDevice->DeviceWrite(lbneReg.diag_channel_input,0x00000000);
	fDevice->DeviceWrite(lbneReg.qi_config,0x0FFF1F00);
	fDevice->DeviceWrite(lbneReg.qi_delay,0x00000000);
	fDevice->DeviceWrite(lbneReg.qi_pulse_width,0x00000000);
	fDevice->DeviceWrite(lbneReg.external_gate_width,0x00008000);
	fDevice->DeviceWrite(lbneReg.dsp_clock_control,0x00000000);

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
