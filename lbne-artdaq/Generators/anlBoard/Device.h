#ifndef DEVICE_H__
#define DEVICE_H__

#include "ftd2xx.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace SSPDAQ{

//PABC defining low-level interface to an SSP board.
//Actual hardware calls must be implemented by derived classes.
class Device{

  //Allow the DeviceManager access to call Open() to prepare
  //the hardware for use. User code must then call 
  //DeviceManager::OpenDevice() to get a pointer to the object
  friend class DeviceManager;

 public:

  virtual ~Device(){};

  //Return whether device is currently open
  virtual bool IsOpen() = 0;

  //Close the device. In order to open the device again, another device needs to be
  //requested from the DeviceManager
  virtual void Close() = 0;

  //Flush communication channel
  virtual void DevicePurgeComm() = 0;

  //Flush data channel
  virtual void DevicePurgeData() = 0;

  //Get number of bytes in data queue (put into numWords)
  virtual void DeviceQueueStatus(unsigned int* numWords) = 0;

  //Read data into vector, up to defined size
  virtual void DeviceReceive(std::vector<unsigned int>& data, unsigned int size) = 0;

  //============================//
  //Read from/write to registers//
  //============================//
  //Where mask is given, only read/write bits which are high in mask

  virtual void DeviceRead(unsigned int address, unsigned int* value) = 0;

  virtual void DeviceReadMask(unsigned int address, unsigned int mask, unsigned int* value) = 0;

  virtual void DeviceWrite(unsigned int address, unsigned int value) = 0;

  virtual void DeviceWriteMask(unsigned int address, unsigned int mask, unsigned int value) = 0;

  //Set bits high in mask to 1
  virtual void DeviceSet(unsigned int address, unsigned int mask) = 0;

  //Set bits high in mask to 0
  virtual void DeviceClear(unsigned int address, unsigned int mask) = 0;

  //Read series of contiguous registers, number to read given in "size"
  virtual void DeviceArrayRead(unsigned int address, unsigned int size, unsigned int* data) = 0;

  //Write series of contiguous registers, number to write given in "size"
  virtual void DeviceArrayWrite(unsigned int address, unsigned int size, unsigned int* data) = 0;

  //=============================

 protected:
  
  bool fSlowControlOnly;

 private:

  //Device can only be opened from the DeviceManager.
  virtual void Open(bool slowControlOnly=false) = 0;

};

}//namespace
#endif
