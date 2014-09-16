#include "EmulatedDevice.h"
#include <cstdlib>
#include "Log.h"
#include "anlExceptions.h"

SSPDAQ::EmulatedDevice::EmulatedDevice(unsigned int deviceNumber){
  fDeviceNumber=deviceNumber;
  isOpen=false;
  fEmulatorThread=0;
}

void SSPDAQ::EmulatedDevice::Open(){
  SSPDAQ::Log::Error()<<"Emulated device open"<<std::endl;
  isOpen=true;
}

void SSPDAQ::EmulatedDevice::Close(){
  error = this->DevicePurgeData();
  error = this->DevicePurgeComm();
  
  isOpen=false;
  SSPDAQ::Log::Error()<<"Emulated Device closed"<<std::endl;
}

int SSPDAQ::EmulatedDevice::DeviceTimeout (unsigned int timeout)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DevicePurgeComm (void)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DevicePurgeData (void)
{
  return 0;
}

//Do something with internal queue buffer here
int SSPDAQ::EmulatedDevice::DeviceQueueStatus (unsigned int* numWords)
{
  return fEmulatedBuffer.size();
}

void SSPDAQ::EmulatedDevice::DeviceReceive(std::vector<unsigned int>& data, unsigned int size){

  unsigned int element=0;
  for(int element=0;element<size){
    bool gotData=fEmulatedBuffer.try_pop(element,1000);
    if(gotData){
      data.push_back(element)
	}
    else{
      break;
    }
  }
}


int SSPDAQ::EmulatedDevice::DeviceReceiveEvent(EventPacket* data, unsigned int* dataReceived){
  return 0;
}

//==============================================================================
// Command Functions
//==============================================================================

//Only respond to specific commands needed to simulate normal operations
int SSPDAQ::EmulatedDevice::DeviceRead (unsigned int address, unsigned int* value)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceReadMask (unsigned int address, unsigned int mask, unsigned int* value)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceWrite (unsigned int address, unsigned int value)
{
  return 0;
  if(address==lbneReg.master_logic_status&&value==0x00000001){
    this->Start();
  }
  else if(address==lbneReg.event_data_control&&value==0x00020001){
    this->Stop();
  }
}

int SSPDAQ::EmulatedDevice::DeviceWriteMask (unsigned int address, unsigned int mask, unsigned int value)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceSet (unsigned int address, unsigned int mask)
{
	return DeviceWriteMask(address, mask, 0xFFFFFFFF);
}

int SSPDAQ::EmulatedDevice::DeviceClear (unsigned int address, unsigned int mask)
{
	return DeviceWriteMask(address, mask, 0x00000000);
}

int SSPDAQ::EmulatedDevice::DeviceArrayRead (unsigned int address, unsigned int size, unsigned int* data)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceArrayWrite (unsigned int address, unsigned int size, unsigned int* data)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceFifoRead (unsigned int address, unsigned int size, unsigned int* data)
{
  return 0;
}

int SSPDAQ::EmulatedDevice::DeviceFifoWrite(unsigned int address, unsigned int size, unsigned int* data)
{
  return 0;
}

//==============================================================
//Emulator-specific functions
//==============================================================

int SSPDAQ::EmulatedDevice::Start(){
  fEmulatorShouldStop=false;
  fEmulatorThread=std::unique_ptr<std::thread>(new std::thread(&SSPDAQ::EmulatedDevice::EmulatorLoop,this));
}

int SSPDAQ::EmulatedDevice::Stop(){
  fEmulatorShouldStop=true;
  fEmulatorThread->join();
}

void SSPDAQ::EmulatedDevice::EmulatorLoop(){

  static unsigned int headerSizeInWords=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
  std::default_random_engine generator;
  std::exponential_distribution<double> timeDistribution(10);
  std::uniform_int_distribution<int> channelDistribution(0,11);

  std::chrono::steady_clock::time_point runStartTime = steady_clock::now();

  while(true){
    double waitTime = timeDistribution(generator);
    usleep(int(waitTime*1.E6));
    std::chrono::steady_clock::time_point eventTime = steady_clock::now();
    unsigned long eventTimestamp = (duration_cast<duration<unsigned long,std::ratio<1, 150000000>>>(eventTime - runStartTime)).count();
    int channel = channelDistribution(generator);

    SSPDAQ::EventHeader header;

    header.header=0xAAAAAAAA;
    header.length=headerSizeInWords;
    header.group1=0x0;
    header.triggerID=0x0;
    header.group2=0x0;
    unsigned long* longPtr=unsigned long*(header.timestamp);
    longPtr[0]=eventTimestamp;
    header.peakSumLow=0x0;
    header.group3=0x0;
    header.preriseLow=0x0;
    header.group4=0x0;
    header.intSumHigh=0x0;
    header.baseline=0x0;
    longPtr=unsigned long*(header.cfdPoint);
    longPtr[0]=0x0;
    longPtr=unsigned long*(header.intTimestamp);
    longPtr[0]=eventTimestamp;

    unsigned int* headerPtr=unsigned int*(&header);
    for(unsigned int element=0;element<headerSizeInWords;++element){
      fEmulatedBuffer.push(headerPtr[element]);
    }
  }
}
