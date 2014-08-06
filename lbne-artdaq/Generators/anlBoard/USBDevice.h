#ifndef USBDEVICE_H__
#define USBDEVICE_H__

#include "anlTypes.h"
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

  friend class DeviceManager;

 public:

  USBDevice(FT_DEVICE_LIST_INFO_NODE* dataChannel, FT_DEVICE_LIST_INFO_NODE* commChannel);

  virtual ~USBDevice(){};

  //Implementation of base class interface

  inline virtual bool IsOpen(){
    return isOpen;
  }

  virtual void Close();

  virtual int DeviceTimeout(unsigned int timeout);

  virtual int DevicePurgeComm();

  virtual int DevicePurgeData();

  virtual int DeviceQueueStatus(unsigned int* numBytes);

  virtual int DeviceReceiveEvent(EventPacket* data, unsigned int* dataReceived);

  virtual void DeviceReceive(std::vector<unsigned int>& data, unsigned int size);

  virtual unsigned int DeviceLostData();

  virtual unsigned int DeviceMissingData();

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

  //Internal functions - make public so debugging code can access them
  int SendReceive(CtrlPacket& tx, CtrlPacket& rx, unsigned int txSize, unsigned int rxSizeExpected, unsigned int retry=retryOn);

  int SendUSB(CtrlPacket& tx, unsigned int txSize);

  int ReceiveUSB(CtrlPacket& rx, unsigned int rxSizeExpected);

  void RxErrorPacket(CtrlPacket& rx, unsigned int status);

 private:

  FT_DEVICE_LIST_INFO_NODE fDataChannel;

  FT_DEVICE_LIST_INFO_NODE fCommChannel;

  bool isOpen;

  unsigned int fDataLost;

  unsigned int fDataMissing;

  virtual void Open();
};

}//namespace
#endif
