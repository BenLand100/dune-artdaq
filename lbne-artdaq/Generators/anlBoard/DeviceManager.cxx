#include "DeviceManager.h"
#include "ftd2xx.h"
#include "Log.h"
#include "anlExceptions.h"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "boost/asio.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

SSPDAQ::DeviceManager& SSPDAQ::DeviceManager::Get(){
  static SSPDAQ::DeviceManager instance;
  return instance;
}

SSPDAQ::DeviceManager::DeviceManager(){
}

std::pair<unsigned int, unsigned int> SSPDAQ::DeviceManager::GetNDevices(){
  if(!fHaveLookedForDevices){
    this->RefreshDevices();
  }
  return std::make_pair(fUSBDevices.size(),fEthernetDevices.size());
}

void SSPDAQ::DeviceManager::RefreshDevices()
{

  for(auto device=fUSBDevices.begin();device!=fUSBDevices.end();++device){
    if(device->IsOpen()){
      SSPDAQ::Log::Warning()<<"Device manager refused request to refresh device list"
			    <<"due to USB devices still open"<<std::endl;
    }
  }
  for(auto device=fEthernetDevices.begin();device!=fEthernetDevices.end();++device){
    if(device->IsOpen()){
      SSPDAQ::Log::Warning()<<"Device manager refused request to refresh device list"
			    <<"due to ethernet devices still open"<<std::endl;
    } 
  }
  for(auto device=fEmulatedDevices.begin();device!=fEmulatedDevices.end();++device){
    if((*device)->IsOpen()){
      SSPDAQ::Log::Warning()<<"Device manager refused request to refresh device list"
			    <<"due to emulated devices still open"<<std::endl;
    } 
  }

  // Clear Device List
  fUSBDevices.clear();
  fEthernetDevices.clear();
  fEmulatedDevices.clear();

  //===========================//
  //==Find USB devices=========//
  //===========================//

  unsigned int ftNumDevs;

  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*> dataChannels;
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*> commChannels;

  // This code uses FTDI's D2XX driver to determine the available devices
  if(FT_CreateDeviceInfoList(&ftNumDevs)!=FT_OK){
    SSPDAQ::Log::Error()<<"Failed to create FTDI device info list"<<std::endl;
    throw(EFTDIError("Error in FT_CreateDeviceInfoList"));
  }

  FT_DEVICE_LIST_INFO_NODE* deviceInfoNodes=new FT_DEVICE_LIST_INFO_NODE[ftNumDevs];

  if(FT_GetDeviceInfoList(deviceInfoNodes,&ftNumDevs)!=FT_OK){
    delete deviceInfoNodes;
    SSPDAQ::Log::Error()<<"Failed to get FTDI device info list"<<std::endl;
    throw(EFTDIError("Error in FT_GetDeviceInfoList"));
  }
  
  //Search through all devices for compatible interfaces
  for (unsigned int i = 0; i < ftNumDevs; i++) {
      // NOTE 1: Each device is actually 2 FTDI devices. Device with "A" at the end of serial number is the
      // data channel. "B" is the comms channel. Need to associate FTDI devices with the same base
      // number together to get a "whole" board.
      //
      // NOTE 2: There is a rare error in which the FTDI driver returns FT_OK but the device info is incorrect
      // In this case, the check on ftType and/or length of ftSerial will fail
      // The discover code will therefore fail to find any useable devices but will not return an error
      // The calling code should check numDevices and rerun FindDevices if it equals zero

      // Search only for devices we can use (skip any others)
      if (deviceInfoNodes[i].Type != FT_DEVICE_2232H) {
	continue;	// Skip to next device
      }
      
      // Add FTDI device to Device List using Serial number
      //If length is zero, then device is probably open in another process (though maybe we don't get type then either...)
      //===TODO: Should check flags for open devices and report the number open in other processes to cout
      unsigned int length = strlen(deviceInfoNodes[i].SerialNumber);	// Find length of serial number (including 'A' or 'B')
      if (length == 0) {
	continue;	// Skip to next device
      }

      char serial[16];

      strncpy(serial, deviceInfoNodes[i].SerialNumber, length - 1);	// Copy base serial number
      serial[length-1] = 0;					// Append NULL because strncpy() didn't!      
      
      // Update device list with FTDI device number
      switch (deviceInfoNodes[i].SerialNumber[length -1]) {
      case 'A':
	dataChannels[serial]=&(deviceInfoNodes[i]);
	break;
      case 'B':
	commChannels[serial]=&(deviceInfoNodes[i]);
	break;
      default:
	break;
      }
  }

  //Check that device list is as expected, then construct USB device objects for each board
  if(dataChannels.size()!=commChannels.size()){
    SSPDAQ::Log::Error()<<"Different number of data and comm channels on FTDI!"<<std::endl;
    delete deviceInfoNodes;
    throw(EBadDeviceList());
  }
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*>::iterator dIter=dataChannels.begin();
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*>::iterator cIter=commChannels.begin();
 
  for(;dIter!=dataChannels.end();++dIter,++cIter){
    if(dIter->first!=cIter->first){
      SSPDAQ::Log::Error()<<"Non-matching serial numbers for data and comm channels on FTDI!"<<std::endl;
      delete deviceInfoNodes;
      throw(EBadDeviceList());
    }
    fUSBDevices.push_back(USBDevice(dIter->second,cIter->second));
    SSPDAQ::Log::Info()<<"Found a device with serial "<<dIter->first<<std::endl;
  }

  delete[] deviceInfoNodes;

  //Test code for boost ethernet library
  /*  std::cout<<"1"<<std::endl;
  boost::asio::io_service io_service;
  std::cout<<"2"<<std::endl;
  //boost::asio::ip::address boardAddress(boost::asio::ip::address_v4::from_string("192.168.1.123"));
  boost::asio::ip::tcp::resolver resolver(io_service);
  boost::asio::ip::tcp::resolver::query query("192.168.1.123", "55001");
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
  boost::asio::ip::tcp::socket boardSocket(io_service);
  boost::asio::connect(boardSocket, endpoint_iterator);

  
  //boost::asio::ip::address boardAddress(boost::asio::ip::address_v4::from_string("127.0.0.1"));
  std::cout<<"3"<<std::endl;
  //  boost::asio::ip::tcp::endpoint boardEndpoint(boardAddress,55001);
  //boost::asio::ip::tcp::endpoint boardEndpoint(boardAddress,22);
  std::cout<<boardEndpoint.address().to_string()<<std::endl;
  std::cout<<boardEndpoint.port()<<std::endl;
  std::cout<<"4"<<std::endl;
  try{
    //boost::asio::ip::tcp::socket boardSocket(io_service,boardEndpoint);
  }
  catch(boost::system::system_error except){
  std::cout<<except.what()<<std::endl;
  }
  SSPDAQ::CtrlPacket tx;
  //  SSPDAQ::CtrlPacket rx;
  unsigned int txSize;
  unsigned int rxSizeExpected;
  
  tx.header.length	= sizeof(CtrlHeader);
  tx.header.address	= 0x40000014; //ARM status, should be 0xABCDEF01 ?
  tx.header.command	= SSPDAQ::cmdRead;
  tx.header.size		= 1;
  tx.header.status	= SSPDAQ::statusNoError;
  txSize			= sizeof(SSPDAQ::CtrlHeader);
  rxSizeExpected		= sizeof(SSPDAQ::CtrlHeader) + sizeof(unsigned int);
  
  usleep(1000);
  std::vector<unsigned int> readBuf(6);
  std::cout<<"5"<<std::endl;
  boardSocket.write_some(boost::asio::buffer((void*)(&tx),txSize));
  std::cout<<"6"<<std::endl;
  usleep(1000);
  boardSocket.read_some(boost::asio::buffer(readBuf,rxSizeExpected));
  std::cout<<"7"<<std::endl;
  for(auto iter=readBuf.begin();iter!=readBuf.end();++iter){
    std::cout<<std::hex<<*iter<<std::dec<<std::endl;
  }
  // Open the event data connection.
  //error = ConnectToTCPServer(&tcpDataHandle, DataPort, DeviceIP, tcpDataCallbackFunction, &tcpDataCallbackData, tcpDataTimeout);
  //if (error) {
  //  error = errorCommConnect;
  //  break;
  //}
  //else
  //{
      // Set the event data interface to Ethernet
  //  DeviceWrite(lbneReg.eventDataInterfaceSelect, 	0x00000001);
  //}    	
  */
}

SSPDAQ::Device* SSPDAQ::DeviceManager::OpenDevice(SSPDAQ::Comm_t commType, unsigned int deviceNum)
{
  //Check for devices if this hasn't yet been done
  if(!fHaveLookedForDevices&&commType!=SSPDAQ::kEmulated){
    this->RefreshDevices();
  }

  Device* device=0;
  switch(commType){
  case SSPDAQ::kUSB:
    device=&fUSBDevices[deviceNum];
    if(device->IsOpen()){
      SSPDAQ::Log::Error()<<"Attempt to open already open device!"<<std::endl;
      throw(EDeviceAlreadyOpen());
    }
    else{
      device->Open();
    }
    break;
  case SSPDAQ::kEthernet:
    SSPDAQ::Log::Error()<<"Ethernet interface not implemented yet!"<<std::endl;
    throw(std::invalid_argument(""));
    break;
  case SSPDAQ::kEmulated:
    while(fEmulatedDevices.size()<=deviceNum){
      fEmulatedDevices.push_back(std::move(std::unique_ptr<SSPDAQ::EmulatedDevice>(new SSPDAQ::EmulatedDevice(fEmulatedDevices.size()))));
    }
    device=fEmulatedDevices[deviceNum].get();
    if(device->IsOpen()){
      SSPDAQ::Log::Error()<<"Attempt to open already open device!"<<std::endl;
      throw(EDeviceAlreadyOpen());
    }
    else{
      device->Open();
    }
    break;
  default:
    SSPDAQ::Log::Error()<<"Unrecognised interface type!"<<std::endl;
    throw(std::invalid_argument(""));
  }
  return device;
}
