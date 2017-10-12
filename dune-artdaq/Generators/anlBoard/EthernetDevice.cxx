#include "EthernetDevice.h"
#include <cstdlib>
#include <algorithm>
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "anlExceptions.h"

boost::asio::io_service SSPDAQ::EthernetDevice::fIo_service;

SSPDAQ::EthernetDevice::EthernetDevice(unsigned long ipAddress):
  isOpen(false),
  fCommSocket(fIo_service),fDataSocket(fIo_service),
  fIP(boost::asio::ip::address_v4(ipAddress))
  {}

void SSPDAQ::EthernetDevice::Open(bool slowControlOnly){

  fSlowControlOnly=slowControlOnly;

  dune::DAQLogger::LogInfo("SSP_EthernetDevice")<<"Looking for SSP Ethernet device at "<<fIP.to_string()<<std::endl;
  boost::asio::ip::tcp::resolver resolver(fIo_service);
  boost::asio::ip::tcp::resolver::query commQuery(fIP.to_string(), slowControlOnly?"55002":"55001");
  boost::asio::ip::tcp::resolver::iterator commEndpointIterator = resolver.resolve(commQuery);
  boost::asio::connect(fCommSocket, commEndpointIterator);
  
  if(slowControlOnly){
    dune::DAQLogger::LogInfo("SSP_EthernetDevice")<<"Connected to SSP Ethernet device at "<<fIP.to_string()<<std::endl;
    return;
  }

  boost::asio::ip::tcp::resolver::query dataQuery(fIP.to_string(), "55010");
  boost::asio::ip::tcp::resolver::iterator dataEndpointIterator = resolver.resolve(dataQuery);
  
  boost::asio::connect(fDataSocket, dataEndpointIterator);
  dune::DAQLogger::LogInfo("SSP_EthernetDevice")<<"Connected to SSP Ethernet device at "<<fIP.to_string()<<std::endl;
}

void SSPDAQ::EthernetDevice::Close(){
  isOpen=false;
  dune::DAQLogger::LogInfo("SSP_EthernetDevice")<<"Device closed"<<std::endl;
} 

void SSPDAQ::EthernetDevice::DevicePurgeComm (void){
  DevicePurge(fCommSocket);
}

void SSPDAQ::EthernetDevice::DevicePurgeData (void){
  DevicePurge(fDataSocket);
}

void SSPDAQ::EthernetDevice::DeviceQueueStatus(unsigned int* numWords){
  unsigned int numBytes=fDataSocket.available();
  (*numWords)=numBytes/sizeof(unsigned int);  
}

void SSPDAQ::EthernetDevice::DeviceReceive(std::vector<unsigned int>& data, unsigned int size){
  data.resize(size);
  unsigned int dataReturned=fDataSocket.read_some(boost::asio::buffer(data));
  if(dataReturned<size*sizeof(unsigned int)){
    data.resize(dataReturned/sizeof(unsigned int));
  }
}

//==============================================================================
// Command Functions
//==============================================================================

void SSPDAQ::EthernetDevice::DeviceRead (unsigned int address, unsigned int* value){
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

void SSPDAQ::EthernetDevice::DeviceReadMask (unsigned int address, unsigned int mask, unsigned int* value)
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

void SSPDAQ::EthernetDevice::DeviceWrite (unsigned int address, unsigned int value)
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

void SSPDAQ::EthernetDevice::DeviceWriteMask (unsigned int address, unsigned int mask, unsigned int value)
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

void SSPDAQ::EthernetDevice::DeviceSet (unsigned int address, unsigned int mask)
{
	DeviceWriteMask(address, mask, 0xFFFFFFFF);
}

void SSPDAQ::EthernetDevice::DeviceClear (unsigned int address, unsigned int mask)
{
	DeviceWriteMask(address, mask, 0x00000000);
}

void SSPDAQ::EthernetDevice::DeviceArrayRead (unsigned int address, unsigned int size, unsigned int* data)
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

void SSPDAQ::EthernetDevice::DeviceArrayWrite (unsigned int address, unsigned int size, unsigned int* data)
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

void SSPDAQ::EthernetDevice::SendReceive(SSPDAQ::CtrlPacket& tx, SSPDAQ::CtrlPacket& rx,
				   unsigned int txSize, unsigned int rxSizeExpected, unsigned int retryCount)
{
  unsigned int timesTried=0;
  bool success=false;

  while(!success){
    try{
      SendEthernet(tx,txSize);
      // Insert small delay between send and receive on Linux
      usleep(100);
      ReceiveEthernet(rx,rxSizeExpected);
      usleep(2000);
      success=true;
    }
    catch(ETCPError){
      if(timesTried<retryCount){
	DevicePurgeComm();
	++timesTried;
	dune::DAQLogger::LogWarning("SSP_EthernetDevice")<<"Send/receive failed "<<timesTried<<" times on Ethernet link, retrying..."<<std::endl;
      }
      else{
	try {
	  dune::DAQLogger::LogError("SSP_EthernetDevice")<<"Send/receive failed on Ethernet link, giving up."<<std::endl;
	} catch (...) {}
	throw;
      }
    }
  }   
}
	
void SSPDAQ::EthernetDevice::SendEthernet(SSPDAQ::CtrlPacket& tx, unsigned int txSize)
{
  unsigned int txSizeWritten=fCommSocket.write_some(boost::asio::buffer((void*)(&tx),txSize));
  if(txSizeWritten!=txSize){
    throw(ETCPError(""));
  }
  
}

void SSPDAQ::EthernetDevice::ReceiveEthernet(SSPDAQ::CtrlPacket& rx, unsigned int rxSizeExpected)
{
  unsigned int rxSizeReturned=fCommSocket.read_some(boost::asio::buffer((void*)(&rx),rxSizeExpected));
  if(rxSizeReturned!=rxSizeExpected){
    throw(ETCPError(""));
  }
}

void SSPDAQ::EthernetDevice::DevicePurge(boost::asio::ip::tcp::socket& socket){
  bool done = false;
  unsigned int bytesQueued = 0;
  unsigned int sleepTime = 0;

  //Keep getting data from channel until queue is empty
  do{
    bytesQueued=socket.available();

    //Read data from device, up to 256 bytes
    if(bytesQueued!=0){
      sleepTime=0;
      unsigned int bytesToGet=std::min((unsigned int)256,bytesQueued);
      std::vector<char> junkBuf(bytesToGet);
      socket.read_some(boost::asio::buffer(junkBuf,bytesToGet));
    }
    //If queue is empty, wait a bit and check that it hasn't filled up again, then return 
    else{
      usleep(1000);	// 1ms
      sleepTime+=1000;
      bytesQueued=socket.available();
      if (bytesQueued == 0&&sleepTime>1000000) {
	done = 1;
      }
    }
  }
  while (!done);
	
}
