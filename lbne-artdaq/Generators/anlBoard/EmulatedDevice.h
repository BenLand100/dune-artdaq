#ifndef EMULATEDDEVICE_H__
#define EMULATEDDEVICE_H__

#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "Device.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace SSPDAQ{

class EmulatedDevice : public Device{

  friend class DeviceManager;

 public:

  EmulatedDevice(unsigned int deviceNumber=0);

  virtual ~EmulatedDevice(){};

  //Implementation of base class interface

  inline virtual bool IsOpen(){
    return isOpen;
  }

  virtual void Close();

  virtual int DeviceTimeout(unsigned int timeout);

  virtual int DevicePurgeComm();

  virtual int DevicePurgeData();

  virtual int DeviceQueueStatus(unsigned int* numWords);

  virtual void DeviceReceive(std::vector<unsigned int>& data, unsigned int size);

  virtual int DeviceRead(unsigned int address, unsigned int* value);

  virtual int DeviceReadMask(unsigned int address, unsigned int mask, unsigned int* value);

  virtual int DeviceWrite(unsigned int address, unsigned int value);

  virtual int DeviceWriteMask(unsigned int address, unsigned int mask, unsigned int value);

  virtual int DeviceSet(unsigned int address, unsigned int mask);

  virtual int DeviceClear(unsigned int address, unsigned int mask);

  virtual int DeviceArrayRead(unsigned int address, unsigned int size, unsigned int* data);

  virtual int DeviceArrayWrite(unsigned int address, unsigned int size, unsigned int* data);

  virtual int DeviceFifoRead(unsigned int address, unsigned int size, unsigned int* data);

  virtual int DeviceFifoWrite(unsigned int address, unsigned int size, unsigned int* data);

 private:

  virtual void Open();
  
  void Start();

  void Stop();

  void EmulatorLoop();

  unsigned int fDeviceNumber;

  bool isOpen;

  unsigned int fDataLost;

  unsigned int fDataMissing;

  std::unique_ptr<std::thread> fEmulatorThread;

  SafeQueue<unsigned int> fEmulatedBuffer;

  std::atomic<bool> fEmulatorShouldStop;

};

}//namespace
#endif
