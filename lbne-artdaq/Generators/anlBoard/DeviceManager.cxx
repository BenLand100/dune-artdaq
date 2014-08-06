#include "DeviceManager.h"
#include "ftd2xx.h"
#include "Log.h"
#include "anlExceptions.h"

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

int SSPDAQ::DeviceManager::RefreshDevices (bool force)
{

  if(!force){
    for(auto device=fUSBDevices.begin();device!=fUSBDevices.end();++device){
      if(device->IsOpen()){
	SSPDAQ::Log::Warning()<<"Device manager refused request to refresh device list"
			      <<"due to USB devices still open"<<std::endl;
	return 1;
      }
    }
    for(auto device=fEthernetDevices.begin();device!=fEthernetDevices.end();++device){
      if(device->IsOpen()){
	SSPDAQ::Log::Warning()<<"Device manager refused request to refresh device list"
			      <<"due to ethernet devices still open"<<std::endl;
	return 1;
      } 
    }
  }
  else{
    SSPDAQ::Log::Warning()<<"Device manager forcibly refreshing devices - will invalidate"
			  <<"pointers to open devices"<<std::endl;
  }

  // Clear Device List
  fUSBDevices.clear();
  fEthernetDevices.clear();

  //===========================//
  //==Find USB devices=========//
  //===========================//

  FT_STATUS ftStatus;
  unsigned int ftNumDevs;

  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*> dataChannels;
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*> commChannels;

  // This code uses FTDI's D2XX driver to determine the available devices
  //===TODO: Change to exception===
  ftStatus = FT_CreateDeviceInfoList(&ftNumDevs);
  if (ftStatus != FT_OK) {
    throw(EFTDIError("Error in FT_CreateDeviceInfoList"));
  }

  FT_DEVICE_LIST_INFO_NODE* deviceInfoNodes=new FT_DEVICE_LIST_INFO_NODE[ftNumDevs];

  ftStatus = FT_GetDeviceInfoList(deviceInfoNodes,&ftNumDevs);
  if (ftStatus != FT_OK) {
    delete deviceInfoNodes;
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

  if(dataChannels.size()!=commChannels.size()){
    SSPDAQ::Log::Error()<<"Different number of data and comm channels!"<<std::endl;
    delete deviceInfoNodes;
    throw(EBadDeviceList());
  }
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*>::iterator dIter=dataChannels.begin();
  std::map<std::string,FT_DEVICE_LIST_INFO_NODE*>::iterator cIter=commChannels.begin();
 
  for(;dIter!=dataChannels.end();++dIter,++cIter){
    if(dIter->first!=cIter->first){
      SSPDAQ::Log::Error()<<"Non-matching serial numbers for data and comm channels!"<<std::endl;
      delete deviceInfoNodes;
      throw(EBadDeviceList());
    }
    fUSBDevices.push_back(USBDevice(dIter->second,cIter->second));
    SSPDAQ::Log::Info()<<"Found a device with serial "<<dIter->first<<std::endl;
  }

  delete[] deviceInfoNodes;

  return 0;
}

SSPDAQ::Device* SSPDAQ::DeviceManager::OpenDevice(DeviceManager::Comm_t commType, unsigned int deviceNum)
{
  if(!fHaveLookedForDevices){
    this->RefreshDevices();
  }

  Device* device=0;
  switch(commType){
  case SSPDAQ::DeviceManager::kUSB:
    device=&fUSBDevices[deviceNum];
    if(device->IsOpen()){
      SSPDAQ::Log::Error()<<"Attempt to open already open device!"<<std::endl;
      throw(EDeviceAlreadyOpen());
    }
    else{
      device->Open();
    }
    break;
  case SSPDAQ::DeviceManager::kEthernet:
    SSPDAQ::Log::Error()<<"Ethernet interface not implemented yet!"<<std::endl;
    throw(std::invalid_argument(""));
  default:
    SSPDAQ::Log::Error()<<"Unrecognised interface type!"<<std::endl;
    throw(std::invalid_argument(""));
  }
  return device;
}
