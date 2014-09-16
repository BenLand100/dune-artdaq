#include "USBDevice.h"
#include <cstdlib>
#include "Log.h"
#include "anlExceptions.h"

SSPDAQ::USBDevice::USBDevice(FT_DEVICE_LIST_INFO_NODE* dataChannel, FT_DEVICE_LIST_INFO_NODE* commChannel){
  fDataChannel=*dataChannel;
  fCommChannel=*commChannel;
  isOpen=false;
}

void SSPDAQ::USBDevice::Open(){

  FT_STATUS ftStatus;
  int error=0;

  const unsigned int ftReadTimeout = 1000;	// in ms
  const unsigned int ftWriteTimeout = 1000;	// in ms
  const unsigned int ftLatency = 2;	// in ms

  const unsigned int dataBufferSize = 0x10000;	// 65536 bytes  - USB Buffer Size on PC
  const unsigned int dataLatency = 2;		// in ms
  const unsigned int dataTimeout	= 1000;		// in ms

  ftStatus=FT_OpenEx(fCommChannel.SerialNumber,FT_OPEN_BY_SERIAL_NUMBER, &(fCommChannel.ftHandle));
  FT_HANDLE commHandle=fCommChannel.ftHandle;
    
  // Setup UART parameters for control path
  if (FT_SetBaudRate(commHandle, FT_BAUD_921600) != FT_OK) {
    error = SSPDAQ::errorCommConnect;
  }
  if (FT_SetDataCharacteristics(commHandle, FT_BITS_8, FT_STOP_BITS_1, FT_PARITY_NONE) != FT_OK) {
    error = SSPDAQ::errorCommConnect;
  }
  if (FT_SetTimeouts(commHandle, ftReadTimeout, ftWriteTimeout) != FT_OK) {
    error = SSPDAQ::errorCommConnect;
  }
  if (FT_SetFlowControl(commHandle, FT_FLOW_NONE, 0, 0) != FT_OK) {
    error = SSPDAQ::errorCommConnect;
  }   
  if (FT_SetLatencyTimer(commHandle, ftLatency) != FT_OK) {
    error = SSPDAQ::errorCommConnect;
  }   
  if (error) {								 
    // Error during control path setup, close already open device and break
    SSPDAQ::Log::Error()<<"Error configuring device! Exiting..."<<std::endl;
    FT_Close(commHandle);
    exit(1);
  }

  ftStatus=FT_OpenEx(fDataChannel.SerialNumber,FT_OPEN_BY_SERIAL_NUMBER, &(fDataChannel.ftHandle));
  FT_HANDLE dataHandle=fDataChannel.ftHandle;

  // Open the Data Path
  if (ftStatus != FT_OK) {
    error = SSPDAQ::errorDataConnect;
    SSPDAQ::Log::Error()<<"Error opening data path"<<std::endl;
    // Since Control Path is already open, close it now
    FT_Close(commHandle);
  }
  SSPDAQ::Log::Error()<<"Opened data path"<<std::endl;
    // Setup parameters for data path
    if (FT_SetLatencyTimer(dataHandle, dataLatency) != FT_OK) {
      error = SSPDAQ::errorDataConnect;
    }
    if (FT_SetUSBParameters(dataHandle, dataBufferSize, dataBufferSize) != FT_OK) {
      error = SSPDAQ::errorDataConnect;
    }
    if (FT_SetTimeouts(dataHandle, dataTimeout, 0) != FT_OK) {
      error = SSPDAQ::errorDataConnect;
    }
    if (FT_SetFlowControl(dataHandle, FT_FLOW_RTS_CTS, 0, 0) != FT_OK) {
      error = SSPDAQ::errorDataConnect;
    }
    SSPDAQ::Log::Error()<<"Written to data path"<<std::endl;
    if (error) {
      // Error during data path setup, close already open devices
      FT_Close(dataHandle);
      FT_Close(commHandle);
      SSPDAQ::Log::Error()<<"Error writing to data path"<<std::endl;
    }
    else{
      SSPDAQ::Log::Error()<<"Device open"<<std::endl;
      isOpen=true;
    }
}

void SSPDAQ::USBDevice::Close(){
  int error = 0;
  FT_STATUS ftStatus;
  // This code uses FTDI's D2XX driver for the control and data paths
  // It seems to be necessary to manually flush the data path RX buffer
  // to avoid crashing LBNEWare when Disconnect is pressed
  error = this->DevicePurgeData();
  error = this->DevicePurgeComm();
  
  ftStatus = FT_Close(fCommChannel.ftHandle);
  if (ftStatus != FT_OK) {
    error = SSPDAQ::errorCommDisconnect;
  }
  ftStatus = FT_Close(fDataChannel.ftHandle);
  if (ftStatus != FT_OK) {
    error = SSPDAQ::errorCommDisconnect;
  }

  if(!error){
    isOpen=false;
    SSPDAQ::Log::Error()<<"Device closed"<<std::endl;
  }
}

int SSPDAQ::USBDevice::DeviceTimeout (unsigned int timeout)
{
	int error = 0;
	FT_STATUS ftStatus;

	ftStatus = FT_SetTimeouts(fDataChannel.ftHandle, timeout, 0);
	if (ftStatus != FT_OK) {
		error = SSPDAQ::errorDataConnect;
	}

	return error;
}

int SSPDAQ::USBDevice::DevicePurgeComm (void)
{
	int		error = 0;
	int		done = 0;
	char	bytes[256];
	unsigned int	bytesQueued = 0;
	unsigned int	bytesReturned = 0;
	FT_STATUS ftStatus;
	
	
	do {
		ftStatus = FT_GetQueueStatus(fCommChannel.ftHandle, &bytesQueued);
		if (ftStatus != FT_OK) {
			error = SSPDAQ::errorCommPurge;
		} else if (bytesQueued > 256) {
			ftStatus = FT_Read(fCommChannel.ftHandle, (void*)&bytes, 256, &bytesReturned);
			if (ftStatus != FT_OK) {
				error = SSPDAQ::errorCommPurge;
			}
		} else if (bytesQueued > 0) {
			ftStatus = FT_Read(fCommChannel.ftHandle, (void*)&bytes, bytesQueued, &bytesReturned);
			if (ftStatus != FT_OK) {
				error = SSPDAQ::errorCommPurge;
			}
		}
		
		if (error) {
			done = 1;
		} else if (bytesQueued == 0) {
		 	usleep(10000);	// 10ms
			ftStatus = FT_GetQueueStatus(fCommChannel.ftHandle, &bytesQueued);
			if (bytesQueued == 0) {
				done = 1;
			}
		}
	} while (!done);

	
	return error;
}

int SSPDAQ::USBDevice::DevicePurgeData (void)
{
	int		error = 0;
	int		done = 0;
	char	bytes[256];
	unsigned int	bytesQueued = 0;
	unsigned int	bytesReturned = 0;
	FT_STATUS ftStatus;
	
	
	do {
	  ftStatus = FT_GetQueueStatus(fDataChannel.ftHandle, &bytesQueued);
	  if (ftStatus != FT_OK) {
	    error = SSPDAQ::errorDataPurge;
	  } else if (bytesQueued > 256) {
	    ftStatus = FT_Read(fDataChannel.ftHandle, (void*)&bytes, 256, &bytesReturned);
	    if (ftStatus != FT_OK) {
	      error = SSPDAQ::errorDataPurge;
	    }
	  } else if (bytesQueued > 0) {
	    ftStatus = FT_Read(fDataChannel.ftHandle, (LPVOID*)&bytes, bytesQueued, &bytesReturned);
	    if (ftStatus != FT_OK) {
	      error = SSPDAQ::errorDataPurge;
	    }
	  }
	  
	  if (error) {
	    done = 1;
	  } else if (bytesQueued == 0) {
	    usleep(10000);	// 10ms
	    ftStatus = FT_GetQueueStatus(fDataChannel.ftHandle, &bytesQueued);
	    if (bytesQueued == 0) {
	      done = 1;
	    }
	  }
	} while (!done);
	
	
	return error;
}

int SSPDAQ::USBDevice::DeviceQueueStatus (unsigned int* numWords)
{
	int error = 0;
	FT_STATUS ftStatus;
	unsigned int* numBytes;
	ftStatus = FT_GetQueueStatus(fDataChannel.ftHandle, numBytes);
	(*numWords)=numBytes/sizeof(unsigned int);
	if (ftStatus != FT_OK) {
		// Error getting Queue Status
		error = SSPDAQ::errorDataQueue;
	}
	
	return error;
}

void SSPDAQ::USBDevice::DeviceReceive(std::vector<unsigned int>& data, unsigned int size){

  FT_STATUS ftStatus;

  unsigned int* buf = new unsigned int[size];
  unsigned int dataReturned;

  ftStatus = FT_Read(fDataChannel.ftHandle, (void*)buf, size*sizeof(unsigned int), &dataReturned);
  if(ftStatus!=FT_OK){
    delete[] buf;
    throw(EFTDIError("FTDI fault on data receive"));
  }
  data.assign(buf,buf+(dataReturned/sizeof(unsigned int)));
  delete[] buf;
}

//==============================================================================
// Command Functions
//==============================================================================

int SSPDAQ::USBDevice::DeviceRead (unsigned int address, unsigned int* value)
{
 	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdRead;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize			= sizeof(SSPDAQ::CtrlHeader);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	
	error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOn);
	if (error == 0) {
		// No Error, return data
		*value = rx.data[0];
	} else {
		// Error, return zero
		*value = 0;
		SSPDAQ::Log::Error() << "DeviceRead Error = " << error << " at " << std::hex << address << std::endl;
	}
	
	return error;
}

int SSPDAQ::USBDevice::DeviceReadMask (unsigned int address, unsigned int mask, unsigned int* value)
{
 	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdReadMask;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= mask;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	
	error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOn);
	if (error == 0) {
		// No Error, return data
		*value = rx.data[0];
	} else {
		// Error, return zero
		*value = 0;
		SSPDAQ::Log::Error() << "DeviceReadMask Error = " << error << " at " << std::hex  << address << std::endl;
	}
	
	return error;
}

int SSPDAQ::USBDevice::DeviceWrite (unsigned int address, unsigned int value)
{
	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdWrite;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= value;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader);

		error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOff);
	if (error != 0) {
		SSPDAQ::Log::Error() << "DeviceWrite Error = " << error << " at " << std::hex  << address << std::endl;
	}

	return error;
}

int SSPDAQ::USBDevice::DeviceWriteMask (unsigned int address, unsigned int mask, unsigned int value)
{
	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdWriteMask;
	tx.header.size		= 1;
	tx.header.status	= SSPDAQ::statusNoError;
	tx.data[0]		= mask;
	tx.data[1]		= value;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * 2);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int); 
	
		error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOff);
	if (error != 0) {
		SSPDAQ::Log::Error() << "DeviceWriteMask Error = " << error << " at " << std::hex  << address << std::endl;
	}

	return error;
}

int SSPDAQ::USBDevice::DeviceSet (unsigned int address, unsigned int mask)
{
	return DeviceWriteMask(address, mask, 0xFFFFFFFF);
}

int SSPDAQ::USBDevice::DeviceClear (unsigned int address, unsigned int mask)
{
	return DeviceWriteMask(address, mask, 0x00000000);
}

int SSPDAQ::USBDevice::DeviceArrayRead (unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
 	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdArrayRead;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize				= sizeof(SSPDAQ::CtrlHeader);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);

	error = SendReceive(tx, rx, txSize, rxSizeExpected, SSPDAQ::retryOff);
	if (error == 0) {
		// No Error, return data
		for (i = 0; i < rx.header.size; i++) {
			data[i] = rx.data[i];
		}
	} else {
		// Error, return zeros
		for (i = 0; i < rx.header.size; i++) {
			data[i] = 0;
		}
		SSPDAQ::Log::Error() << "DeviceArrayRead Error = " << error << " at " << std::hex  << address << std::endl;
	}
	
	return error;
}

int SSPDAQ::USBDevice::DeviceArrayWrite (unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
 	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdArrayWrite;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize				= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader);
	
	for (i = 0; i < size; i++) {
		tx.data[i] = data[i];
	}

	error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOff);
	if (error != 0) {
		SSPDAQ::Log::Error() << "DeviceArrayWrite Error = " << error << " at " << std::hex  << address << std::endl;
	}
	
	return error;
}

int SSPDAQ::USBDevice::DeviceFifoRead (unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
 	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdFifoRead;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize				= sizeof(SSPDAQ::CtrlHeader);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);

	error = SendReceive(tx, rx, txSize, rxSizeExpected, retryOff);
	if (error == 0) {
		// No Error, return data
		for (i = 0; i < rx.header.size; i++) {
			data[i] = rx.data[i];
		}
	} else {
		// Error, return zeros
		for (i = 0; i < rx.header.size; i++) {
			data[i] = 0;
		}
		SSPDAQ::Log::Error() << "DeviceFifoRead Error = " << error << " at " << std::hex  << address << std::endl;
	}

	return error;
}

int SSPDAQ::USBDevice::DeviceFifoWrite(unsigned int address, unsigned int size, unsigned int* data)
{
	unsigned int i = 0;
	int error = 0;
	SSPDAQ::CtrlPacket tx;
	SSPDAQ::CtrlPacket rx;
	unsigned int txSize;
	unsigned int rxSizeExpected;

	tx.header.address	= address;
	tx.header.command	= SSPDAQ::cmdFifoWrite;
	tx.header.size		= size;
	tx.header.status	= SSPDAQ::statusNoError;
	txSize			= sizeof(SSPDAQ::CtrlHeader) + (sizeof(unsigned int) * size);
	rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader); 
	
	for (i = 0; i < size; i++) {
		tx.data[i] = data[i];
	}

	error = SendReceive(tx, rx, txSize, rxSizeExpected, SSPDAQ::retryOff);
	if (error != 0) {
		SSPDAQ::Log::Error() << "DeviceFifoWrite Error = " << error << " at " << std::hex  << address << std::endl;
	}
	
	return error;
}

//==============================================================================
// Support Functions
//==============================================================================

int SSPDAQ::USBDevice::SendReceive(SSPDAQ::CtrlPacket& tx, SSPDAQ::CtrlPacket& rx,
				   unsigned int txSize, unsigned int rxSizeExpected, unsigned int retry)
{
	int error = 0;
	
	if (error == 0) {
	  error = SendUSB(tx,txSize);
		// Insert small delay between send and receive on Linux
	  		usleep(100);
	}

	if (error == 0) {
	  error = ReceiveUSB(rx,rxSizeExpected);
		// Insert small delay between transactions on Linux
	  	usleep(2000);
	}
	else{
	  RxErrorPacket(rx, statusSendError);
	}
	
	if ((error == SSPDAQ::errorCommReceiveZero) || (error == SSPDAQ::errorCommReceiveTimeout)) {
	  if (retry == SSPDAQ::retryOn) {

	    error = SendUSB(tx,txSize);
	    // Insert small delay between send and receive on Linux
	    usleep(100);
	    
	    if (error == 0) {
	      error = ReceiveUSB(rx,rxSizeExpected);
	      // Insert small delay between transactions of Linux
	        usleep(2000);
	    }
	    else{      
	      RxErrorPacket(rx, SSPDAQ::statusSendError);
	    }
	    
	  }
	}
		
	return error;
}
	
int SSPDAQ::USBDevice::SendUSB (SSPDAQ::CtrlPacket& tx, unsigned int txSize)
{
	int error = 0;
	FT_STATUS ftStatus;
	unsigned int txSizeWritten;
	// Send TX data over FTDI control path
	ftStatus = FT_Write(fCommChannel.ftHandle, (char*)&tx, txSize, &txSizeWritten);
	if (ftStatus != FT_OK) {
		// Error during send, return error packet
		error = SSPDAQ::errorCommSend;
		
	} else if (txSizeWritten == 0) {
		// Timeout after sending nothing, return error packet
		// BUGBUG: In this case, we could probably retry the send
		error = SSPDAQ::errorCommSendZero;
	} else if (txSizeWritten != txSize) {
		// Timeout after sending less than expected
		// BUGBUG: In this case, the if the device received enough of the packet
		// it would still respond.  So long as the call to FT_Purge works, we
		// shouldn't get out of sync.
		error = SSPDAQ::errorCommSendTimeout;
	}
	
	return error;
}

int SSPDAQ::USBDevice::ReceiveUSB (SSPDAQ::CtrlPacket& rx, unsigned int rxSizeExpected)
{
	int error = 0;
	FT_STATUS ftStatus;
	unsigned int rxSizeReturned;

	// Request RX data over FTDI control path
	ftStatus = FT_Read(fCommChannel.ftHandle, (void*)&rx, rxSizeExpected, &rxSizeReturned);
	if (ftStatus != FT_OK) {
		// Error during receive, return error packet
		error = SSPDAQ::errorCommReceive;
		RxErrorPacket(rx, SSPDAQ::statusReceiveError);
	} else if (rxSizeReturned == 0) {
		// Timeout after receiving nothing, return error packet
		error = SSPDAQ::errorCommReceiveZero;
		RxErrorPacket(rx, SSPDAQ::statusTimeoutError);
	} else if (rxSizeReturned != rxSizeExpected) {
		// Timeout after receiving less than expected
		// Most likely, device returned an error packet (just a header)
		error = SSPDAQ::errorCommReceiveTimeout;
	}
	return error;
}

void SSPDAQ::USBDevice::RxErrorPacket (SSPDAQ::CtrlPacket& rx, unsigned int status) {
	unsigned int i = 0;

	rx.header.address	= 0;
	rx.header.command	= SSPDAQ::cmdNone;
	rx.header.size		= 0;
	rx.header.status	= status;
	for (i = 0; i < MAX_CTRL_DATA; i++) {
		rx.data[i] = 0;
	}
}
