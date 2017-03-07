#ifndef USBDEVICE_H__
#define USBDEVICE_H__

#include "dune-raw-data/Overlays/anlTypes.hh"
#include "Device.h"
#include "ftd2xx.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace SSPDAQ{

class USBDevice : public Device{

  private:

    //Hardcoded timeouts, latencies, buffer sizes

  static const unsigned int ftReadTimeout = 1000;	// in ms
  static const unsigned int ftWriteTimeout = 1000;	// in ms
  static const unsigned int ftLatency = 2;	// in ms

  static const unsigned int dataBufferSize = 0x10000;	// 65536 bytes  - USB Buffer Size on PC
  static const unsigned int dataLatency = 2;		// in ms
  static const unsigned int dataTimeout	= 1000;		// in ms

 friend class DeviceManager;

 public:

 //Create a device object using FTDI handles given for data and communication channels
 USBDevice(FT_DEVICE_LIST_INFO_NODE* dataChannel, FT_DEVICE_LIST_INFO_NODE* commChannel);

 virtual ~USBDevice(){};
 
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

  //Internal functions - make public so debugging code can access them
  
  void SendReceive(CtrlPacket& tx, CtrlPacket& rx, unsigned int txSize, unsigned int rxSizeExpected, unsigned int retryCount=0);

  void SendUSB(CtrlPacket& tx, unsigned int txSize);

  void ReceiveUSB(CtrlPacket& rx, unsigned int rxSizeExpected);

 private:

  //FTDI handle to data channel
  FT_DEVICE_LIST_INFO_NODE fDataChannel;

  //FTDI handle to comms channel
  FT_DEVICE_LIST_INFO_NODE fCommChannel;

  bool isOpen;

  //Can only be opened by DeviceManager, not by user
  virtual void Open(bool slowControlOnly=false);

  //Function called by DevicePurgeComm and DevicePurgeData
  void DevicePurge(FT_DEVICE_LIST_INFO_NODE& channel);
};

}//namespace
#endif
