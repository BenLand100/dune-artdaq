#ifndef DEVICEINTERFACE_H__
#define DEVICEINTERFACE_H__

#include "DeviceManager.h"
#include "Device.h"
#include "dune-raw-data/Overlays/anlTypes.hh"
#include "SafeQueue.h"
#include "EventPacket.h"
#include <string>

namespace SSPDAQ{

  class DeviceInterface{
    
  public:

    enum State_t{kUninitialized,kRunning,kStopping,kStopped,kBad};

    //Just sets the fields needed to request the device.
    //Real work is done in Initialize which is called manually.
    DeviceInterface(SSPDAQ::Comm_t commType, unsigned long deviceId);

    void OpenSlowControl();

    //Does all the real work in connecting to and setting up the device
    void Initialize();

    //Start a run :-)
    //If you want to read events manually using ReadEventFromDevice, disable
    //the read thread and the DeviceInterface will not read events or build
    //millislices.
    void Start(bool startReadThread=true);

    //Pop a millislice from fQueue and place into sliceData
    void GetMillislice(std::vector<unsigned int>& sliceData);

    //Stop a run. Also resets device state and purges buffers.
    //This is called automatically by Initialize().
    void Stop();

    //Relinquish control of device, which must already be stopped.
    //Allows opening hardware in another interface object if needed.
    void Shutdown();

    //Build a sensible default configuration (What I got from Michael
    //along with Ethernet interface code). Artdaq should do everything
    //in fhicl - this method is for convenience when running test code.
    void Configure();

    //Called by ReadEvents
    //Get an event off the hardware buffer.
    //Timeout after some wait period
    void ReadEventFromDevice(EventPacket& event);

    //Obtain current state of device
    inline State_t State(){return fState;}

    //Setter for single register
    //If mask is given then only bits which are high in the mask will be set.
    void SetRegister(unsigned int address, unsigned int value, unsigned int mask=0xFFFFFFFF);

    //Setter for series of contiguous registers, with vector input
    void SetRegisterArray(unsigned int address, std::vector<unsigned int> value);

    //Setter for series of contiguous registers, with C array input
    void SetRegisterArray(unsigned int address, unsigned int* value, unsigned int size);

    //Getter for single register
    //If mask is set then bits which are low in the mask will be returned as zeros.
    void ReadRegister(unsigned int address, unsigned int& value, unsigned int mask=0xFFFFFFFF);

    //Getter for series of contiguous registers, with vector output
    void ReadRegisterArray(unsigned int address, std::vector<unsigned int>& value, unsigned int size);

    //Getter for series of contiguous registers, with C array output
    void ReadRegisterArray(unsigned int address, unsigned int* value, unsigned int size);

    //Methods to set registers with names (as defined in SSPDAQ::RegMap)

    //Set single named register
    void SetRegisterByName(std::string name, unsigned int value);

    //Set single element of an array of registers
    void SetRegisterElementByName(std::string name, unsigned int index, unsigned int value);
      
    //Set all elements of an array to a single value
    void SetRegisterArrayByName(std::string name, unsigned int value);
      
    //Set all elements of an array using values vector
    void SetRegisterArrayByName(std::string name, std::vector<unsigned int> values);

    //Read single named register
    void ReadRegisterByName(std::string name, unsigned int& value);

    //Read single element of an array of registers
    void ReadRegisterElementByName(std::string name, unsigned int index, unsigned int& value);
      
    //Read all elements of an array into values vector
    void ReadRegisterArrayByName(std::string name, std::vector<unsigned int>& values);

    void SetMillisliceLength(unsigned int length){fMillisliceLength=length;}

    void SetMillisliceOverlap(unsigned int length){fMillisliceOverlap=length;}

    void SetEmptyWriteDelayInus(unsigned int time){fEmptyWriteDelayInus=time;}

    void SetHardwareClockRateInMHz(unsigned int rate){fHardwareClockRateInMHz=rate;}

    void SetUseExternalTimestamp(bool val){fUseExternalTimestamp=val;}

    void SetStartOnNOvASync(bool val){fStartOnNOvASync=val;}

    std::string GetIdentifier();

    bool exception() const { return exception_.load(); }

  private:
    
    //Internal device object used for hardware operations.
    //Owned by the device manager, not this object.
    Device* fDevice;

    //Whether we are using USB or Ethernet to connect to the device
    SSPDAQ::Comm_t fCommType;

    //Index of the device in the hardware-returned list
    unsigned long fDeviceId;

    //Holds current device state. Hopefully this matches the state of the
    //hardware itself.
    State_t fState;

    //Flag telling read thread to stop
    std::atomic<bool> fShouldStop;

    //Call at Start. Will read events from device and monitor for
    //millislice boundaries
    void ReadEvents(unsigned long runStartTime=0);

    //Called by ReadEvents
    //Build millislice from events in buffer and place in fQueue
    void BuildMillislice(const std::vector<EventPacket>& events,unsigned long startTime,unsigned long endTime);

    //Build a millislice containing only a header and place in fQueue
    void BuildEmptyMillislice(unsigned long startTime,unsigned long endTime);

    // JCF, Mar-8-2016

    // Rather than throw an exception (crashing the enclosing artdaq
    // process) signal that this object is in an exception state via an
    // atomic boolean

    void set_exception( bool exception ) { exception_.store( exception ); }

    SafeQueue<std::vector<unsigned int> > fQueue;

    std::unique_ptr<std::thread> fReadThread;

    unsigned long fMillislicesSent;

    unsigned long fMillislicesBuilt;

    unsigned int fMillisliceLength;

    unsigned int fMillisliceOverlap;

    unsigned int fUseExternalTimestamp;
    
    unsigned int fHardwareClockRateInMHz;

    unsigned int fEmptyWriteDelayInus;

    bool fSlowControlOnly;

    bool fStartOnNOvASync;

    std::atomic<bool> exception_;

  };
  
}//namespace
#endif
