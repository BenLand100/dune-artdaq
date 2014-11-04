#include "DeviceInterface.h"
#include "anlExceptions.h"
#include "Log.h"
#include "RegMap.h"
#include <time.h>
#include <utility>

SSPDAQ::DeviceInterface::DeviceInterface(SSPDAQ::Comm_t commType, unsigned int deviceId)
  : fCommType(commType), fDeviceId(deviceId), fState(SSPDAQ::DeviceInterface::kUninitialized){
  fReadThread=0;
}

void SSPDAQ::DeviceInterface::Initialize(){

  //Ask device manager for a pointer to the specified device
  SSPDAQ::DeviceManager& devman=SSPDAQ::DeviceManager::Get();

  SSPDAQ::Device* device=0;

  SSPDAQ::Log::Info()<<"Initializing "<<((fCommType==SSPDAQ::kUSB)?"USB":((fCommType==SSPDAQ::kEthernet)?"Ethernet":"Emulated"))
		     <<" device #"<<fDeviceId<<"..."<<std::endl;
  
  device=devman.OpenDevice(fCommType,fDeviceId);
  
  if(!device){
    SSPDAQ::Log::Error()<<"Unable to get handle to device; giving up!"<<std::endl;
    throw(ENoSuchDevice());
  }

  fDevice=device;

  //Put device into sensible state
  this->Stop();
  this->Configure();
}

void SSPDAQ::DeviceInterface::Stop(){
  if(fState!=SSPDAQ::DeviceInterface::kRunning&&
     fState!=SSPDAQ::DeviceInterface::kUninitialized){
    SSPDAQ::Log::Warning()<<"Running stop command for non-running device!"<<std::endl;
  }
  SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
      
  fDevice->DeviceWrite(lbneReg.eventDataControl, 0x0013001F);
  fDevice->DeviceClear(lbneReg.master_logic_status, 0x00000001);
  // Clear the FIFOs
  fDevice->DeviceWrite(lbneReg.fifo_control, 0x08000000);
  // Reset the links and flags				
  fDevice->DeviceWrite(lbneReg.event_data_control, 0x00020001);
  // Flush RX buffer in USB chip and driver
  fDevice->DevicePurgeData();
  fState=SSPDAQ::DeviceInterface::kStopped;
  fShouldStop=true;

  if(fReadThread){
    fReadThread->join();
    fReadThread.reset();
  }  

}

void SSPDAQ::DeviceInterface::Configure(){

  if(fState!=kStopped){
    SSPDAQ::Log::Warning()<<"Attempt to reconfigure non-stopped device refused!"<<std::endl;
    return;
  }

  SSPDAQ::RegMap& lbneReg = SSPDAQ::RegMap::Get();
  // Setting up some constants to use during initialization
  const unsigned int nChannels          = 12;
  const unsigned int module_id          = 0xABC;	// This value is reported in the event header
  const unsigned int led_threshold	= 25;	
  const unsigned int cfd_fraction	= 0x1800;
  const unsigned int readout_pretrigger	= 100;	
  const unsigned int event_packet_length= 2046;	
  const unsigned int p_window		= 0;	
  const unsigned int k_window		= 20;
  const unsigned int m1_window		= 10;	
  const unsigned int m2_window		= 5;	
  const unsigned int d_window		= 20;
  const unsigned int i_window		= 300;
  const unsigned int disc_width		= 10;
  const unsigned int baseline_start	= 0x0000;
  const unsigned int baseline_delay	= 5;
  const unsigned int trigger_config	= 0x00000000;

  /*  const unsigned int	chan_config[nChannels] = 
    {
      //      0x00F0E061,		// configure channel #0 in a slow timestamp triggered mode
      0x00005001,               // put channel #0 into external trigger mode
      //0x80F00801,               //put channel #0 into internal led trigger mode
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
      };*/

  const unsigned int	chan_config[nChannels] = 
    {
      //      0x00F0E061,		// configure channel #0 in a slow timestamp triggered mode
      0x00006001,               // put channel #0 into external trigger mode
      //0x80F00801,               //put channel #0 into internal led trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      0x00006001,               // put channel #0 into external trigger mode
      };

  //	Channel Configuration Bit Descriptions
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
  //	0		channel enable
  //

  unsigned int i = 0;
  unsigned int data[nChannels];


  //Set clock source to NOvA clock
  fDevice->DeviceWrite(0x80000520,0x13);
  //Set front panel trigger input to active low
  fDevice->DeviceWrite(0x80000408,0x1101);

  fDevice->DeviceWrite(lbneReg.c2cControl, 0x00000007);
  fDevice->DeviceWrite(lbneReg.clockControl, 0x00000001);
  fDevice->DeviceWrite(lbneReg.module_id, module_id);
  fDevice->DeviceWrite(lbneReg.c2c_intr_control, 0x00000000);
  for (i = 0; i < nChannels; i++) data[i] = chan_config[i];
  fDevice->DeviceArrayWrite(lbneReg.control_status[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = led_threshold;
  fDevice->DeviceArrayWrite(lbneReg.led_threshold[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = cfd_fraction;
  fDevice->DeviceArrayWrite(lbneReg.cfd_parameters[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = readout_pretrigger;
  fDevice->DeviceArrayWrite(lbneReg.readout_pretrigger[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = event_packet_length;
  fDevice->DeviceArrayWrite(lbneReg.readout_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = p_window;
  fDevice->DeviceArrayWrite(lbneReg.p_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = k_window;
  fDevice->DeviceArrayWrite(lbneReg.k_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = m1_window;
  fDevice->DeviceArrayWrite(lbneReg.m1_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = m2_window;
  fDevice->DeviceArrayWrite(lbneReg.m2_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = d_window;
  fDevice->DeviceArrayWrite(lbneReg.d_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = i_window;
  fDevice->DeviceArrayWrite(lbneReg.i_window[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = disc_width;
  fDevice->DeviceArrayWrite(lbneReg.disc_width[0], nChannels, data);
  for (i = 0; i < nChannels; i++) data[i] = baseline_start;
  fDevice->DeviceArrayWrite(lbneReg.baseline_start[0], nChannels, data);

  fDevice->DeviceWrite(lbneReg.gpio_output_width, 0x00001000);
  fDevice->DeviceWrite(lbneReg.led_config, 0x00000000);
  fDevice->DeviceWrite(lbneReg.baseline_delay, baseline_delay);
  fDevice->DeviceWrite(lbneReg.diag_channel_input, 0x00000000);
  fDevice->DeviceWrite(lbneReg.trigger_config, trigger_config);
  // Load the window settings - This MUST be the last operation
  fDevice->DeviceWrite(lbneReg.channel_pulsed_control, 0x1);
}

void SSPDAQ::DeviceInterface::Start(){

  if(fState!=kStopped){
    SSPDAQ::Log::Warning()<<"Attempt to start acquisition on non-stopped device refused!"<<std::endl;
    return;
  }

  std::cout<<"Device interface starting run"<<std::endl;
  SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
  // This script enables all logic and FIFOs and starts data acquisition in the device
  // Operations MUST be performed in this order
  fDevice->DeviceWrite(lbneReg.event_data_control, 0x00000000);
  // Release the FIFO reset						
  fDevice->DeviceWrite(lbneReg.fifo_control, 0x00000000);
  // Registers in the Zynq FPGA (Comm)
  // Reset the links and flags (note eventDataControl!=event_data_control)
  fDevice->DeviceWrite(lbneReg.eventDataControl, 0x00000000);
  // Registers in the Artix FPGA (DSP)
  // Release master logic reset & enable active channels
  fDevice->DeviceWrite(lbneReg.master_logic_status, 0x00000001);

  fShouldStop=false;
  fState=SSPDAQ::DeviceInterface::kRunning;
  std::cout<<"Device interface starting read thread...";

  fReadThread=std::unique_ptr<std::thread>(new std::thread(&SSPDAQ::DeviceInterface::ReadEvents,this));
    std::cout<<"Run started!"<<std::endl;
}

void SSPDAQ::DeviceInterface::ReadEvents(){

  if(fState!=kRunning){
    SSPDAQ::Log::Warning()<<"Attempt to get data from non-running device refused!"<<std::endl;
    return;
  }

  //REALLY needs to be set up to know real run start time.
  //Think the idea was to tell the fragment generators when to start
  //taking data, so we would store this and throw away events occuring
  //before the start time.
  //Doesn't matter for emulated data since this does start at t=0.
  unsigned long runStartTime=0;
  unsigned long millisliceStartTime=runStartTime;
  unsigned long millisliceLengthInTicks=100000000;//0.67s
  unsigned long millisliceOverlapInTicks=10000000;//0.067s
  unsigned int millisliceCount=0;

  //Need two lots of event packets, to manage overlap between slices.
  std::vector<SSPDAQ::EventPacket> events_thisSlice;
  std::vector<SSPDAQ::EventPacket> events_nextSlice;

  //Check whether other thread has set the stop flag
  while(!fShouldStop){
    SSPDAQ::EventPacket event;
    //Ask for event and check that one was returned.
    //ReadEventFromDevice Will return an empty packet with header word set to 0xDEADBEEF
    //if there was no event to read from the SSP.
    this->ReadEventFromDevice(event);
    if(event.header.header!=0xAAAAAAAA){
      usleep(100);//0.1ms
      continue;
    }

    //Convert 4*unsigned shorts into 1*unsigned long event timestamp
    unsigned long eventTime=0;
    for(unsigned int iWord=0;iWord<=3;++iWord){
      eventTime+=((unsigned long)(event.header.timestamp[iWord]))<<16*(3-iWord);
    }

    //Event fits into the current slice
    //Add to current slice only
    if(eventTime<millisliceStartTime+millisliceLengthInTicks){
      events_thisSlice.push_back(std::move(event));
    }
    //Event is in next slice, but in the overlap window of the current slice
    //Add to both slices
    else if(eventTime<millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks){
      events_thisSlice.push_back(std::move(event));
      events_nextSlice.push_back(events_thisSlice.back());
    }
    //Event is not in overlap window of current slice
    else{
      std::cout<<"Device interface building millislice with "<<events_thisSlice.size()<<" events"<<std::endl;
      //Build a millislice based on the existing events
      //and swap next-slice event list into current-slice list
      this->BuildMillislice(events_thisSlice,millisliceStartTime,millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks);
      events_thisSlice.clear();
      std::swap(events_thisSlice,events_nextSlice);
      millisliceStartTime+=millisliceLengthInTicks;
      ++millisliceCount;

      //Maybe current event doesn't go into next slice either...
      while(eventTime>millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks){
	//Write next millislice if it is not empty (i.e. there were some events in overlap period)
	if(events_thisSlice.size()){
	  this->BuildMillislice(events_thisSlice,millisliceStartTime,millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks);
	  events_thisSlice.clear();
	}
	//Then just write empty millislices until we get to the slice which contains this event
	else{
	  this->BuildEmptyMillislice(millisliceStartTime,millisliceStartTime+millisliceLengthInTicks+millisliceOverlapInTicks);
	}
	++millisliceCount;
	millisliceStartTime+=millisliceLengthInTicks;
      }

      //Start collecting events into the next non-empty slice
      events_thisSlice.push_back(std::move(event));
      //If this event is in overlap period put it into both slices
      if(eventTime>millisliceStartTime+millisliceLengthInTicks){
	events_nextSlice.push_back(events_thisSlice.back());
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

  std::cout<<"Pushing slice with "<<events.size()<<" triggers onto queue!"<<std::endl;
  fQueue.push(std::move(sliceData));
}

void SSPDAQ::DeviceInterface::BuildEmptyMillislice(unsigned long startTime, unsigned long endTime){
  std::vector<SSPDAQ::EventPacket> emptySlice;
  this->BuildMillislice(emptySlice,startTime,endTime);
}

void SSPDAQ::DeviceInterface::GetMillislice(std::vector<unsigned int>& sliceData){
  fQueue.try_pop(sliceData,std::chrono::microseconds(100000)); //Try to pop from queue for 100ms
}

void SSPDAQ::DeviceInterface::ReadEventFromDevice(EventPacket& event){
  
  if(fState!=kRunning){
    SSPDAQ::Log::Warning()<<"Attempt to get data from non-running device refused!"<<std::endl;
    event.SetEmpty();
    return;
  }

  std::vector<unsigned int> data;

  unsigned int skippedWords=0;

  //Find first word in event header (0xAAAAAAAA)
  while(true){
    fDevice->DeviceReceive(data,1);

    //If no data is available in pipe then return
    //without filling packet
    if(data.size()==0){
      if(skippedWords){
	SSPDAQ::Log::Warning()<<"Warning: GetEvent skipped "<<skippedWords<<"words "
			      <<"and has not seen header for next event!"<<std::endl;
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
      ++skippedWords;
    }
  }

  if(skippedWords){
    SSPDAQ::Log::Warning()<<"Warning: GetEvent skipped "<<skippedWords<<"words "
			<<"before finding next event header!"<<std::endl;
  }
  
  
  unsigned int* headerBlock=(unsigned int*)&event.header;
  headerBlock[0]=0xAAAAAAAA;
  
  static const unsigned int headerReadSize=(sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)-1);

  //Wait for hardware queue to fill with full header data
  unsigned int queueLengthInUInts=0;
  unsigned int timeWaited=0;//in us

  do{
    fDevice->DeviceQueueStatus(&queueLengthInUInts);
    if(queueLengthInUInts<headerReadSize){
      usleep(10); //10us
      timeWaited+=10;
      if(timeWaited>1000000){ //1s
	SSPDAQ::Log::Error()<<"SSP delayed 1s between issuing header word and full header; giving up"
			    <<std::endl;
	event.SetEmpty();
	throw(EEventReadError());
      }
    }
  }while(queueLengthInUInts<headerReadSize);

  //Get header from device and check it is the right length
  fDevice->DeviceReceive(data,headerReadSize);
  if(data.size()!=headerReadSize){
    SSPDAQ::Log::Error()<<"SSP returned truncated header even though FIFO queue is of sufficient length!"
			<<std::endl;
    event.SetEmpty();
    throw(EEventReadError());
  }

  //Copy header into event packet
  std::copy(data.begin(),data.end(),&(headerBlock[1]));

  SSPDAQ::EventHeader* hPtr=reinterpret_cast<SSPDAQ::EventHeader*>(headerBlock);

  unsigned long eventTime=0;

  for(unsigned int iWord=0;iWord<=3;++iWord){
    eventTime+=((unsigned long)(hPtr->timestamp[iWord]))<<16*(3-iWord);
  }
  std::cout<<"Interface building an event with timestamp "<<eventTime<<std::endl;
    
  //Wait for hardware queue to fill with full event data
  unsigned int bodyReadSize=event.header.length-(sizeof(EventHeader)/sizeof(unsigned int));
  queueLengthInUInts=0;
  timeWaited=0;//in us

  do{
    fDevice->DeviceQueueStatus(&queueLengthInUInts);
    if(queueLengthInUInts<bodyReadSize){
      usleep(10); //10us
      timeWaited+=10;
      if(timeWaited>1000000){ //1s
	SSPDAQ::Log::Error()<<"SSP delayed 1s between issuing header and full event; giving up"
			    <<std::endl;
	event.SetEmpty();
	throw(EEventReadError());
      }
    }
  }while(queueLengthInUInts<bodyReadSize);
   
  //Get event from SSP and check that it is the right length
  fDevice->DeviceReceive(data,bodyReadSize);

  if(data.size()!=bodyReadSize){
    SSPDAQ::Log::Error()<<"SSP returned truncated event even though FIFO queue is of sufficient length!"
			<<std::endl;
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
    SSPDAQ::Log::Error()<<"Request to set named register array "<<name<<", length "<<reg.Size()
			<<"with vector of "<<values.size()<<" values!"<<std::endl;
    throw(std::invalid_argument(""));
  }
  this->SetRegisterArray(reg,values);
}
