#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "ftd2xx.h"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "USBDevice.h"
#include "EmulatedDevice.h"

#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <memory>
#include "lbne-raw-data/Overlays/anlTypes.hh"

namespace SSPDAQ{

class DeviceManager{

 public:
  
  //Get reference to instance of DeviceManager singleton
  static DeviceManager& Get();

  //return pair containging <N_USBDevices,N_EthernetDevices>
  //TODO: should really report serial numbers of available devices
  std::pair<unsigned int, unsigned int> GetNDevices();

  //Open a device and return a pointer containing a handle to it
  Device* OpenDevice(Comm_t commType,unsigned int deviceId);

  //Interrogate FTDI for list of devices. GetNDevices and OpenDevice will call this
  //if it has not yet been run, so it should not normally be necessary to call this directly.
  void RefreshDevices();

 private:

  DeviceManager();

  DeviceManager(DeviceManager const&); //Don't implement

  void operator=(DeviceManager const&); //Don't implement

  //List of USB devices on FTDI link
  std::vector<USBDevice> fUSBDevices;

  //List of ethernet devices (interface not implemented yet)
  std::vector<Device> fEthernetDevices;

  //List of emulated devices
  std::vector<std::unique_ptr<EmulatedDevice> > fEmulatedDevices;

  bool fHaveLookedForDevices;
};

}//namespace
#endif
