#ifndef EMULATEDDEVICE_H__
#define EMULATEDDEVICE_H__

#include "dune-raw-data/Overlays/anlTypes.hh"
#include "Device.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <memory>
#include "SafeQueue.h"
#include <atomic>

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

  virtual void DevicePurgeComm();

  virtual void DevicePurgeData();

  virtual void DeviceQueueStatus(unsigned int* numWords);

  virtual void DeviceReceive(std::vector<unsigned int>& data, unsigned int size);

  virtual void DeviceRead(unsigned int address, unsigned int* value);

  virtual void DeviceReadMask(unsigned int address, unsigned int mask, unsigned int* value);

  virtual void DeviceWrite(unsigned int address, unsigned int value);

  virtual void DeviceWriteMask(unsigned int address, unsigned int mask, unsigned int value);

  virtual void DeviceSet(unsigned int address, unsigned int mask);

  virtual void DeviceClear(unsigned int address, unsigned int mask);

  virtual void DeviceArrayRead(unsigned int address, unsigned int size, unsigned int* data);

  virtual void DeviceArrayWrite(unsigned int address, unsigned int size, unsigned int* data);

 private:

  virtual void Open(bool slowControlOnly=false);

  //Start generation of events by emulator thread
  //Called when appropriate register is set via DeviceWrite
  void Start();

  //Stop generation of events by emulator thread
  //Called when appropriate register is set via DeviceWrite
  void Stop();

  //Add fake events to fEmulatedBuffer periodically
  void EmulatorLoop();

  //Device number to put into event headers
  unsigned int fDeviceNumber;

  bool isOpen;

  //Separate thread to generate fake data asynchronously
  std::unique_ptr<std::thread> fEmulatorThread;

  //Buffer for fake data, popped from by DeviceReceive
  SafeQueue<unsigned int> fEmulatedBuffer;

  //Set by Stop method; tells emulator thread to stop generating data
  std::atomic<bool> fEmulatorShouldStop;
};

}//namespace
#endif
