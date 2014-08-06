#include "DeviceInterface.h"
#include "anlExceptions.h"
#include "Log.h"
#include "RegMap.h"
#include <time.h>
#include <utility>

SSPDAQ::DeviceInterface::DeviceInterface(SSPDAQ::DeviceManager::Comm_t commType, unsigned int deviceId)
  : fCommType(commType), fDeviceId(deviceId), fState(SSPDAQ::DeviceInterface::kUninitialized){
}

void SSPDAQ::DeviceInterface::Initialize(){
  SSPDAQ::DeviceManager& devman=SSPDAQ::DeviceManager::Get();

  SSPDAQ::Device* device=0;

  SSPDAQ::Log::Info()<<"Initializing "<<((fCommType==SSPDAQ::DeviceManager::kUSB)?"USB":"Ethernet")
		     <<" device #"<<fDeviceId<<"..."<<std::endl;

  unsigned int tries=0;

  while(!device&&tries<4){
    if(tries){
      SSPDAQ::Log::Warning()<<"Device initialization failed "<<tries<<" times.."<<std::endl;
    }
  
    device=devman.OpenDevice(fCommType,fDeviceId);
  }

  if(!device){
    SSPDAQ::Log::Error()<<"Unable to get handle to device; giving up!"<<std::endl;
    throw(ENoSuchDevice());
  }

  fDevice=device;
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

  const unsigned int	chan_config[nChannels] = 
    {
      0x00F0E061,		// configure channel #0 in a slow timestamp triggered mode
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

  SSPDAQ::RegMap& lbneReg=SSPDAQ::RegMap::Get();
  // This script enables all logic and FIFOs and starts data acquisition in the device
  // Operations MUST be performed in this order
  fDevice->DeviceWrite(lbneReg.event_data_control, 0x00000000);
  // Release the FIFO reset						
  fDevice->DeviceWrite(lbneReg.fifo_control, 0x00000000);
  // Registers in the Zynq FPGA (Comm)
  // Reset the links and flags
  fDevice->DeviceWrite(lbneReg.eventDataControl, 0x00000000);
  // Registers in the Artix FPGA (DSP)
  // Release master logic reset & enable active channels
  fDevice->DeviceSet(lbneReg.master_logic_status, 0x00000001);

  fState=SSPDAQ::DeviceInterface::kRunning;
}

void SSPDAQ::DeviceInterface::GetEvent(EventPacket& event){
  
  if(fState!=kRunning){
    SSPDAQ::Log::Warning()<<"Attempt to get data from non-running device refused!"<<std::endl;
    return;
  }

  std::vector<unsigned int> data;
  unsigned int* headerBlock=(unsigned int*)&event.header;

  //Needs fixing to time in ms
  time_t tStart(0), tEnd(0);
  time(&tStart); time(&tEnd);
  bool headerFound=false;
  unsigned int skippedWords=0;

  //Find first word in event header (0xAAAAAAAA)
  while(tEnd-tStart<2){
    fDevice->DeviceReceive(data,1);

    if (data.size()!=0&&data[0]==0xAAAAAAAA){
      headerFound=true;
      break;
    }
    time(&tEnd);
    if(data.size()>0){
      ++skippedWords;
    }
  }

  if(skippedWords){
    SSPDAQ::Log::Warning()<<"Warning: GetEvent skipped "<<skippedWords<<"words "
			<<"before finding next event header!"<<std::endl;
  }
  
  //Load rest of header
  if(!headerFound){
    SSPDAQ::Log::Warning()<<"Warning: GetEvent timed out without finding a valid header."
			  <<" Returning empty event."<<std::endl;
    event.SetEmpty();
    return;
  }

  headerBlock[0]=0xAAAAAAAA;
  
  static const unsigned int headerReadSize=(sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)-1);

  fDevice->DeviceReceive(data,headerReadSize);
  if(data.size()!=headerReadSize){
    SSPDAQ::Log::Warning()<<"Warning: GetEvent found truncated header!"<<std::endl;
    //Should check queue size first and throw if data is not returned even for sufficient queue length
  }

  std::copy(data.begin(),data.end(),&(headerBlock[1]));

  unsigned int bodyReadSize=event.header.length-(sizeof(EventHeader)/sizeof(unsigned int));
  fDevice->DeviceReceive(data,bodyReadSize);

  if(data.size()!=bodyReadSize){
    SSPDAQ::Log::Warning()<<"Warning: GetEvent found truncated event!"<<std::endl;
  }

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

void SSPDAQ::DeviceInterface::SetRegister(unsigned int address, std::vector<unsigned int> value){

    this->SetRegister(address,&(value[0]),value.size());
}

void SSPDAQ::DeviceInterface::SetRegister(unsigned int address, unsigned int* value, unsigned int size){

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

void SSPDAQ::DeviceInterface::ReadRegister(unsigned int address, std::vector<unsigned int>& value, unsigned int size){

  value.resize(size);
  this->ReadRegister(address,&(value[0]),size);
}

void SSPDAQ::DeviceInterface::ReadRegister(unsigned int address, unsigned int* value, unsigned int size){

  fDevice->DeviceArrayRead(address,size,value);
}
