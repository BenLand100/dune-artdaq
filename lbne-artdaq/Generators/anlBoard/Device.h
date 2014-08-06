#ifndef DEVICE_H__
#define DEVICE_H__

#include "ftd2xx.h"
#include "EventPacket.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace SSPDAQ{

//PABC defining low-level interface to an ANL board.
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

  //Close the device. In order to open the device again, another
  virtual void Close() = 0;

  //Set read timeout of data link in ms. 
  //Write operations do not currently time out.
  virtual int DeviceTimeout(unsigned int timeout) = 0;

  //Flush communication channel
  virtual int DevicePurgeComm() = 0;

  //Flush data channel
  virtual int DevicePurgeData() = 0;

  //Get number of bytes in data queue (put into numBytes)
  virtual int DeviceQueueStatus(unsigned int* numBytes) = 0;

  //Read data 
  virtual void DeviceReceive(std::vector<unsigned int>& data, unsigned int size) = 0;

  //Read data 
  virtual int DeviceReceiveEvent(EventPacket* data, unsigned int* dataReceived) = 0;

  virtual unsigned int DeviceLostData() = 0;

  virtual unsigned int DeviceMissingData() = 0;

  virtual int DeviceRead(unsigned int address, unsigned int* value) = 0;

  virtual int DeviceReadMask(unsigned int address, unsigned int mask, unsigned int* value) = 0;

  virtual int DeviceWrite(unsigned int address, unsigned int value) = 0;

  virtual int DeviceWriteMask(unsigned int address, unsigned int mask, unsigned int value) = 0;

  virtual int DeviceSet(unsigned int address, unsigned int mask) = 0;

  virtual int DeviceClear(unsigned int address, unsigned int mask) = 0;

  virtual int DeviceArrayRead(unsigned int address, unsigned int size, unsigned int* data) = 0;

  virtual int DeviceArrayWrite(unsigned int address, unsigned int size, unsigned int* data) = 0;

  virtual int DeviceFifoRead(unsigned int address, unsigned int size, unsigned int* data) = 0;

  virtual int DeviceFifoWrite(unsigned int address, unsigned int size, unsigned int* data) = 0;

 private:

  //Device can only be opened from the DeviceManager.
  virtual void Open() = 0;

};

}//namespace
#endif
