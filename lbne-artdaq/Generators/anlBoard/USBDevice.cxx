#include "USBDevice.h"
#include <cstdlib>
#include <algorithm>
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"
#include "anlExceptions.h"

SSPDAQ::USBDevice::USBDevice(FT_DEVICE_LIST_INFO_NODE* dataChannel, FT_DEVICE_LIST_INFO_NODE* commChannel){
  fDataChannel=*dataChannel;
  fCommChannel=*commChannel;
  isOpen=false;
}

void SSPDAQ::USBDevice::Open(bool slowControlOnly){

  fSlowControlOnly=slowControlOnly;

  if(FT_OpenEx(fCommChannel.SerialNumber,FT_OPEN_BY_SERIAL_NUMBER, &(fCommChannel.ftHandle)) != FT_OK){
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error opening USB comm path"<<std::endl;
    } catch (...) {}
    throw(SSPDAQ::EFTDIError("Failed to open USB comm channel"));
  }

  // Setup parameters for control path
  // Parameter values mostly hardcoded in USBDevice header
  bool hasFailed=false;

  FT_HANDLE commHandle=fCommChannel.ftHandle;
  if (FT_SetBaudRate(commHandle, FT_BAUD_921600) != FT_OK) {
    hasFailed=true;
  }
  if (FT_SetDataCharacteristics(commHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK) {
    hasFailed=true;
  }
  if (FT_SetTimeouts(commHandle, ftReadTimeout, ftWriteTimeout) != FT_OK) {
    hasFailed=true;
  }
  if (FT_SetFlowControl(commHandle, FT_FLOW_NONE, 0, 0) != FT_OK) {
    hasFailed=true;
  }   
  if (FT_SetLatencyTimer(commHandle, ftLatency) != FT_OK) {
    hasFailed=true;
  }   
  if (hasFailed) {								 
    // Error during control path setup, close already open device and throw
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error configuring USB comm channel parameters!"<<std::endl;
    } catch (...) {}
    FT_Close(commHandle);
    throw(SSPDAQ::EFTDIError("Failed to configure USB comm channel"));
  }
  lbne::DAQLogger::LogInfo("SSP_USBDevice")<<"Opened control path"<<std::endl;
  if(slowControlOnly){
    lbne::DAQLogger::LogInfo("SSP_USBDevice")<<"Device open!"<<std::endl;
    isOpen=true;
    return;
  }

  // Open the Data Path
  if (FT_OpenEx(fDataChannel.SerialNumber,FT_OPEN_BY_SERIAL_NUMBER, &(fDataChannel.ftHandle)) != FT_OK) {
    //Error opening data path; close already-open comm path and throw 
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error opening USB data path"<<std::endl;
    } catch (...) {}
    FT_Close(commHandle);
    throw(SSPDAQ::EFTDIError("Failed to open USB data channel"));
  }

  FT_HANDLE dataHandle=fDataChannel.ftHandle;
  lbne::DAQLogger::LogInfo("SSP_USBDevice")<<"Opened data path"<<std::endl;
  
  // Setup parameters for data path
    if (FT_SetLatencyTimer(dataHandle, dataLatency) != FT_OK) {
      hasFailed=true;
    }
    if (FT_SetUSBParameters(dataHandle, dataBufferSize, dataBufferSize) != FT_OK) {
      hasFailed=true;
    }
    if (FT_SetTimeouts(dataHandle, dataTimeout, 0) != FT_OK) {
      hasFailed=true;
    }
    if (FT_SetFlowControl(dataHandle, FT_FLOW_RTS_CTS, 0, 0) != FT_OK) {
      hasFailed=true;
    }
    if (hasFailed) {
      // Error during data path setup, close already open devices
      FT_Close(dataHandle);
      FT_Close(commHandle);
      try {
	lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error configuring USB data channel parameters"<<std::endl;
      } catch (...) {}
      throw(SSPDAQ::EFTDIError("Failed to configure USB data channel"));
    }
    else{
      lbne::DAQLogger::LogInfo("SSP_USBDevice")<<"Device open!"<<std::endl;
      isOpen=true;
    }
}

void SSPDAQ::USBDevice::Close(){

  // This code uses FTDI's D2XX driver for the control and data paths
  // It seems to be necessary to manually flush the data path RX buffer
  // to avoid crashing LBNEWare when Disconnect is pressed
  if(!fSlowControlOnly){
    this->DevicePurgeData();
  }
  this->DevicePurgeComm();
  
  if(FT_Close(fCommChannel.ftHandle)!=FT_OK){
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"Failed to close USB comm channel"<<std::endl;
    } catch (...) {}
    throw(SSPDAQ::EFTDIError("Failed to close comm channel"));
  }

  if(!fSlowControlOnly){
    if(FT_Close(fDataChannel.ftHandle)!=FT_OK){
      try {
	lbne::DAQLogger::LogError("SSP_USBDevice")<<"Failed to close USB data channel"<<std::endl;
      } catch (...) {}
      throw(SSPDAQ::EFTDIError("Failed to close data channel"));
    }  
  }
  isOpen=false;
  lbne::DAQLogger::LogInfo("SSP_USBDevice")<<"Device closed"<<std::endl;
} 

void SSPDAQ::USBDevice::DevicePurgeComm (void)
{
  DevicePurge(fCommChannel);
}

void SSPDAQ::USBDevice::DevicePurgeData (void)
{
  DevicePurge(fDataChannel);
}

void SSPDAQ::USBDevice::DeviceQueueStatus(unsigned int* numWords)
{
  unsigned int numBytes;
  if(FT_GetQueueStatus(fDataChannel.ftHandle, &numBytes)!=FT_OK){
    try {
    lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error getting queue length from USB device"<<std::endl;
    } catch (...) {}
    throw(EFTDIError("Error getting queue length from USB device"));
  }
  (*numWords)=numBytes/sizeof(unsigned int);
  
}

void SSPDAQ::USBDevice::DeviceReceive(std::vector<unsigned int>& data, unsigned int size){

  unsigned int* buf = new unsigned int[size];
  unsigned int dataReturned;

  if(FT_Read(fDataChannel.ftHandle, (void*)buf, size*sizeof(unsigned int), &dataReturned)!=FT_OK){
    delete[] buf;
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"FTDI fault on data receive"<<std::endl;
    } catch (...) {}
    throw(EFTDIError("FTDI fault on data receive"));
  }

  data.assign(buf,buf+(dataReturned/sizeof(unsigned int)));
  delete[] buf;
}

//==============================================================================
// Command Functions
//==============================================================================

void SSPDAQ::USBDevice::DeviceRead (unsigned int address, unsigned int* value)
{
 	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdRead;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize			= sizeof(SSPDAQ::CtrlHeader);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);

	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
	*value = rx.data[0];
}

void SSPDAQ::USBDevice::DeviceReadMask (unsigned int address, unsigned int mask, unsigned int* value)
{
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader) + sizeof(uint);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdReadMask;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= mask;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	
	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
	*value = rx.data[0];
}

void SSPDAQ::USBDevice::DeviceWrite (unsigned int address, unsigned int value)
{
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader) + sizeof(uint);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdWrite;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= value;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader);

	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
}

void SSPDAQ::USBDevice::DeviceWriteMask (unsigned int address, unsigned int mask, unsigned int value)
{
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader) + (sizeof(uint) * 2);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdWriteMask;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= mask;
	tx.data[1]		= value;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * 2);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int); 
	
	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
}

void SSPDAQ::USBDevice::DeviceSet (unsigned int address, unsigned int mask)
{
	DeviceWriteMask(address, mask, 0xFFFFFFFF);
}

void SSPDAQ::USBDevice::DeviceClear (unsigned int address, unsigned int mask)
{
	DeviceWriteMask(address, mask, 0x00000000);
}

void SSPDAQ::USBDevice::DeviceArrayRead (unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdArrayRead;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize				= sizeof(SSPDAQ::CtrlHeader);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);

	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
	for (i = 0; i < rx.header.size; i++) {
	  data[i] = rx.data[i];
	}	
}

void SSPDAQ::USBDevice::DeviceArrayWrite (unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
 	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.length	= sizeof(CtrlHeader) + (sizeof(uint) * size);
	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdArrayWrite;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize				= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader);
	
	for (i = 0; i < size; i++) {
		tx.data[i] = data[i];
	}

	SendReceive(tx, rx, txSize, rxSizeExpected, 3);
}

//==============================================================================
// Support Functions
//==============================================================================

void SSPDAQ::USBDevice::SendReceive(SSPDAQ::CtrlPacket& tx, SSPDAQ::CtrlPacket& rx,
				   unsigned int txSize, unsigned int rxSizeExpected, unsigned int retryCount)
{
  unsigned int timesTried=0;
  bool success=false;

  while(!success){
    try{
      SendUSB(tx,txSize);
      // Insert small delay between send and receive on Linux
      usleep(100);
      ReceiveUSB(rx,rxSizeExpected);
      usleep(2000);
      success=true;
    }
    catch(EFTDIError){
      if(timesTried<retryCount){
	DevicePurgeComm();
	++timesTried;
	lbne::DAQLogger::LogWarning("SSP_USBDevice")<<"Send/receive failed "<<timesTried<<" times on USB link, retrying..."<<std::endl;
      }
      else{
	try {
	lbne::DAQLogger::LogError("SSP_USBDevice")<<"Send/receive failed on USB link, giving up."<<std::endl;
	} catch (...) {}
	throw;
      }
    }
  }   
}
	
void SSPDAQ::USBDevice::SendUSB (SSPDAQ::CtrlPacket& tx, unsigned int txSize)
{
	unsigned int txSizeWritten;
	// Send TX data over FTDI control path
	if(FT_Write(fCommChannel.ftHandle, (char*)&tx, txSize, &txSizeWritten)!=FT_OK
	   ||txSizeWritten!=txSize){
	  try {
	    lbne::DAQLogger::LogError("SSP_USBDevice")<<"Failed to send data on USB comm channel!"<<std::endl;
	  } catch (...) {}
	  throw(EFTDIError("Failed to send data on USB comm channel"));
	}
}

void SSPDAQ::USBDevice::ReceiveUSB (SSPDAQ::CtrlPacket& rx, unsigned int rxSizeExpected)
{
	unsigned int rxSizeReturned;
	auto errorCode=FT_Read(fCommChannel.ftHandle, (void*)&rx, rxSizeExpected, &rxSizeReturned);
	// Request RX data over FTDI control path
	if(errorCode!=FT_OK
	   ||rxSizeReturned!=rxSizeExpected){
	  try {
	    lbne::DAQLogger::LogError("SSP_USBDevice")<<"Failed to receive data on USB comm channel!"<<std::endl;
	  } catch (...) {}
	  throw(EFTDIError("Failed to receive data on USB comm channel"));
	}
}

void SSPDAQ::USBDevice::DevicePurge(FT_DEVICE_LIST_INFO_NODE& channel){
  bool hasFailed = false;
  bool done = false;
  char buf[256];
  unsigned int bytesQueued = 0;
  unsigned int bytesReturned = 0;
    
  //Keep getting data from channel until queue is empty
  do {

    //Interrogate device for queue length
    if(FT_GetQueueStatus(channel.ftHandle, &bytesQueued)!=FT_OK){
      hasFailed = true;
      break;
    }

    //Read data from device, up to 256 bytes
    if(bytesQueued!=0){
      unsigned int bytesToGet=std::min((unsigned int)256,bytesQueued);
      
      if(FT_Read(channel.ftHandle, (void*)&buf, bytesToGet, &bytesReturned)!=FT_OK){
	hasFailed=true;
	break;
      }
    }
    //If queue is empty, wait a bit and check that it hasn't filled up again, then return 
    else{
      usleep(10000);	// 10ms
      if(FT_GetQueueStatus(channel.ftHandle, &bytesQueued)!=FT_OK){
	hasFailed=true;
	break;
      }
      if (bytesQueued == 0) {
	done = 1;
      }
    }
  } while (!done);

  if(hasFailed){
    try {
      lbne::DAQLogger::LogError("SSP_USBDevice")<<"Error purging USB device queue"<<std::endl;
    } catch (...) {}
    throw(EFTDIError("Error purging USB device queue"));
  }
	
}
