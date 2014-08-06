#ifndef DEVICEMANAGER_H
#define DEVICEMANAGER_H

#include "ftd2xx.h"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "USBDevice.h"

#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <string>
#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace SSPDAQ{

class DeviceManager{

 public:
  
  enum Comm_t{kUSB, kEthernet};

  static DeviceManager& Get();
  std::pair<unsigned int, unsigned int> GetNDevices();
  Device* OpenDevice(Comm_t commType,unsigned int deviceId);

 private:

  void ReleaseDevice(Device*);
  int RefreshDevices(bool force=false);
  DeviceManager();
  DeviceManager(DeviceManager const&); //Don't implement
  void operator=(DeviceManager const&); //Don't implement

  std::vector<USBDevice> fUSBDevices;
  std::vector<Device> fEthernetDevices;
  bool fHaveLookedForDevices;
};

}//namespace
#endif
