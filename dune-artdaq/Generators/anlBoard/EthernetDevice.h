#ifndef ETHERNETDEVICE_H__
#define ETHERNETDEVICE_H__

#include "dune-raw-data/Overlays/anlTypes.hh"
#include "Device.h"
#include "boost/asio.hpp"

#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace SSPDAQ{

class EthernetDevice : public Device{

  private:

 friend class DeviceManager;

 public:

 //Create a device object using FTDI handles given for data and communication channels
 EthernetDevice(unsigned long ipAddress);

 virtual ~EthernetDevice(){};
 
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

  void SendEthernet(CtrlPacket& tx, unsigned int txSize);

  void ReceiveEthernet(CtrlPacket& rx, unsigned int rxSizeExpected);

  void DevicePurge(boost::asio::ip::tcp::socket& socket);

 private:

  bool isOpen;

  static boost::asio::io_service fIo_service;

  boost::asio::ip::tcp::socket fCommSocket;
  boost::asio::ip::tcp::socket fDataSocket;

  boost::asio::ip::address fIP;

  //Can only be opened by DeviceManager, not by user
  virtual void Open(bool slowControlOnly=false);

};

}//namespace
#endif
