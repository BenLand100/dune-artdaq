#ifndef DEVICEINTERFACE_H__
#define DEVICEINTERFACE_H__

#include "DeviceManager.h"
#include "Device.h"
#include "lbne-raw-data/Overlays/anlTypes.hh"

namespace SSPDAQ{

  class DeviceInterface{
    
  public:

    enum State_t{kUninitialized,kRunning,kStopping,kStopped,kBad};

    //Just sets the fields needed to request the device.
    //Real work is done in Initialize which is called manually.
    DeviceInterface(DeviceManager::Comm_t commType, unsigned int deviceId);

    //Does all the real work in connecting to and setting up the device
    void Initialize();

    //Start a run :-)
    void Start();

    //Stop a run. Also resets device state and purges buffers.
    //This is called automatically by Initialize().
    void Stop();

    //Get an event off the hardware buffer.
    //Need to change this to use threading and a software buffer.
    void GetEvent(EventPacket& event);

    //Relinquish control of device, which must already be stopped.
    //Allows opening hardware in another interface object if needed.
    void Shutdown();

    //Just enable a default configuration for now.
    //Need to get some info from ANL guys about what
    //we might actually want to change...
    void Configure();

    //Obtain current state of device
    inline State_t State(){return fState;}

    //Setter for single register
    //If mask is given then only bits which are high in the mask will be set.
    void SetRegister(unsigned int address, unsigned int value, unsigned int mask=0xFFFFFFFF);

    //Setter for series of contiguous registers, with vector input
    void SetRegister(unsigned int address, std::vector<unsigned int> value);

    //Setter for series of contiguous registers, with C array input
    void SetRegister(unsigned int address, unsigned int* value, unsigned int size);

    //Getter for single register
    //If mask is set then bits which are low in the mask will be returned as zeros.
    void ReadRegister(unsigned int address, unsigned int& value, unsigned int mask=0xFFFFFFFF);

    //Getter for series of contiguous registers, with vector output
    void ReadRegister(unsigned int address, std::vector<unsigned int>& value, unsigned int size);

    //Getter for series of contiguous registers, with C array output
    void ReadRegister(unsigned int address, unsigned int* value, unsigned int size);
    
  private:
    
    //Internal device object used for hardware operations.
    //Owned by the device manager, not this object.
    Device* fDevice;

    //Whether we are using USB or Ethernet to connect to the device
    DeviceManager::Comm_t fCommType;

    //Index of the device in the hardware-returned list
    unsigned int fDeviceId;

    //Holds current device state. Hopefully this matches the state of the
    //hardware itself.
    State_t fState;

  };
  
}//namespace
#endif
