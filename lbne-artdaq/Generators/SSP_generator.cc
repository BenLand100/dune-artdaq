#include "lbne-artdaq/Generators/SSP.hh"
#include "lbne-artdaq/Generators/anlBoard/anlExceptions.h"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragmentWriter.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>

lbne::SSP::SSP(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  fragment_type_(lbne::detail::PHOTON),
  board_id_(ps.get<unsigned int>("board_id",0))
{
  instance_name_for_metrics_ = "SSP " + boost::lexical_cast<std::string>(board_id_);


  unsigned int interfaceTypeCode(ps.get<unsigned int>("interface_type",999));

  switch(interfaceTypeCode){
  case 0:
    interface_type_=SSPDAQ::kUSB;
    break;
  case 1:
    interface_type_=SSPDAQ::kEthernet;
    break;
  case 2:
    interface_type_=SSPDAQ::kEmulated;
    break;
  case 999:
    throw art::Exception(art::errors::Configuration)
      <<"Interface type not defined in configuration fhicl file!\n";
  default:
    throw art::Exception(art::errors::Configuration)
      <<"Unknown interface type code "
      <<interfaceTypeCode
      <<".\n";
  }

  //Awful hack to get two devices to work together for now
  if(interface_type_!=1){
    board_id_=0;//std::stol(board_id_str);
  }
  else{
    board_id_=inet_network(ps.get<std::string>("board_ip").c_str());
  }
  device_interface_=new SSPDAQ::DeviceInterface(interface_type_,board_id_);//board_id_);
      device_interface_->Initialize();
  this->ConfigureDevice(ps);
  this->ConfigureDAQ(ps);
}

void lbne::SSP::ConfigureDevice(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet hardwareConfig( ps.get<fhicl::ParameterSet>("HardwareConfig") );
  std::vector<std::string> hcKeys=hardwareConfig.get_keys();

  //Special case for channel_control register - first we
  //will get the literal register values, and then look
  //for logical settings in the .fcl and replace bits as appropriate.
  std::vector<unsigned int> chControlReg(12,0);
  bool haveChannelControl=false;

  for(auto hcIter=hcKeys.begin();hcIter!=hcKeys.end();++hcIter){
    
    //Deal with this later on
    if(!hcIter->compare("ChannelControl")){
      haveChannelControl=true;
    }

    //Expect to see a Literals section; parse these literal registers
    else if(!hcIter->compare("Literals")){
      fhicl::ParameterSet literalRegisters( hardwareConfig.get<fhicl::ParameterSet>("Literals") );
      std::vector<std::string> lrKeys=literalRegisters.get_keys();

      for(auto lrIter=lrKeys.begin();lrIter!=lrKeys.end();++lrIter){
	std::vector<unsigned int> vals=literalRegisters.get<std::vector<unsigned int> >(*lrIter);

	unsigned int regVal=vals[0];
	unsigned int regMask=vals.size()>1?vals[1]:0xFFFFFFFF;
	unsigned int regAddress=0;
	std::istringstream(*lrIter)>>regAddress;
	
	device_interface_->SetRegister(regAddress,regVal,regMask);
      }
    }//End Processing of Literals

    //Intercept channel_control setting so that we can replace bits with logical values later...
    else if(!hcIter->substr(4).compare("channel_control")){
      if(!hcIter->substr(0,4).compare("ELE_")){
	std::vector<unsigned int> vals=hardwareConfig.get<std::vector<unsigned int> >(*hcIter);
	chControlReg[vals[0]]=vals[1];
      }
      else if(!hcIter->substr(0,4).compare("ALL_")){ //All array elements set to same value
	unsigned int val=hardwareConfig.get<unsigned int>(*hcIter);
	for(unsigned int i=0;i<12;++i){
	  chControlReg[i]=val;
	}
      }
      else if(!hcIter->substr(0,4).compare("ARR_")){ //All array elements individually
	std::vector<unsigned int> vals=hardwareConfig.get<std::vector<unsigned int> >(*hcIter);
	for(unsigned int i=0;i<12;++i){
	  chControlReg[i]=vals[i];
	}
      }
    }

    else if(!hcIter->substr(0,4).compare("ELE_")){ //Single array element
      std::vector<unsigned int> vals=hardwareConfig.get<std::vector<unsigned int> >(*hcIter);
      device_interface_->SetRegisterElementByName(hcIter->substr(4,std::string::npos),vals[0],vals[1]);
    }
    else if(!hcIter->substr(0,4).compare("ALL_")){ //All array elements set to same value
      unsigned int val=hardwareConfig.get<unsigned int>(*hcIter);
      device_interface_->SetRegisterArrayByName(hcIter->substr(4,std::string::npos),val);
    }
    else if(!hcIter->substr(0,4).compare("ARR_")){ //All array elements individually
      std::vector<unsigned int> vals=hardwareConfig.get<std::vector<unsigned int> >(*hcIter);
      device_interface_->SetRegisterArrayByName(hcIter->substr(4,std::string::npos),vals);
    }
    else{ //Individual register not in an array
      unsigned int val=hardwareConfig.get<unsigned int>(*hcIter);
      device_interface_->SetRegisterByName(*hcIter,val);
    }
  }

  //Modify channel control registers and send to hardware
  if(haveChannelControl){
    fhicl::ParameterSet channelControl( hardwareConfig.get<fhicl::ParameterSet>("ChannelControl"));
    this->BuildChannelControlRegisters(channelControl,chControlReg);
  }
  device_interface_->SetRegisterArrayByName("channel_control",chControlReg);
}

void lbne::SSP::BuildChannelControlRegisters(fhicl::ParameterSet const& ps,std::vector<unsigned int>& reg){
  std::vector<std::string> ccKeys=ps.get_keys();

  for(auto ccIter=ccKeys.begin();ccIter!=ccKeys.end();++ccIter){

    //External trigger mode
    if(!ccIter->compare("ExtTriggerMode")){
      unsigned int val=ps.get<unsigned int>(*ccIter);
      switch(val){
      case 0:                          //No external trigger
	for(unsigned int i=0;i<12;++i){
	  reg[i]=reg[i]&0xFFFF0FFF;
	}
	break;
      case 1:                          //Edge trigger on front panel
	for(unsigned int i=0;i<12;++i){
	  reg[i]=(reg[i]&0xFFFF0FFF)+0x00006000;
	}
	break;
      case 2:                          //Use front panel as gate
	for(unsigned int i=0;i<12;++i){
	  reg[i]=(reg[i]&0xFFFF0FFF)+0x00005000;
	}
	break;
      case 3:                          //Timestamp trigger
	for(unsigned int i=0;i<12;++i){
	  reg[i]=(reg[i]&0xFFFF0FFF)+0x0000E000;
	}
	break;
      default:
	try {
	  DAQLogger::LogError("SSP_SSP_generator")<<"Error: invalid value for external trigger source setting!"<<std::endl;
	} catch (...) {}
	throw SSPDAQ::EDAQConfigError("");
      }
    }
    else if(!ccIter->compare("LEDTrigger")){
      unsigned int val=ps.get<unsigned int>(*ccIter);
      switch(val){
      case 1:                          //Negative edge
	for(unsigned int i=0;i<12;++i){
	  reg[i]=(reg[i]&0x7FFFF3FF)+0x80000800;
	}
	break;
      case 2:                          //Positive edge
	for(unsigned int i=0;i<12;++i){
	  reg[i]=(reg[i]&0x7FFFF3FF)+0x80000400;
	}
        break;
      case 0:
	for(unsigned int i=0;i<12;++i){//Disabled
          reg[i]=reg[i]&0x7FFFF3FF;
        }
	break;
      default:
	try {
	  DAQLogger::LogError("SSP_SSP_generator")<<"Error: invalid value for trigger polarity setting!"<<std::endl;
	} catch (...) {}
	throw SSPDAQ::EDAQConfigError("");
      }
    }
    else if(!ccIter->compare("TimestampRate")){
      unsigned int val=ps.get<unsigned int>(*ccIter);
      if(val>7){
	try {
	  DAQLogger::LogError("SSP_SSP_generator")<<"Error: invalid value for timestamp rate setting!"<<std::endl;
	} catch (...) {}
	throw SSPDAQ::EDAQConfigError("");
      }
      for(unsigned int i=0;i<12;++i){
	reg[i]=(reg[i]&0xFFFFFF1F)+0x20*val;
      }
    }
  }
}

void lbne::SSP::ConfigureDAQ(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet daqConfig( ps.get<fhicl::ParameterSet>("DAQConfig") );

  unsigned int millisliceLength=daqConfig.get<unsigned int>("MillisliceLength",0);

  if(millisliceLength==0){
    
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Millislice length not defined in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int millisliceOverlap=daqConfig.get<unsigned int>("MillisliceOverlap",0);
  /*
  if(millisliceOverlap==0){
  try {
    DAQLogger::LogError("SSP_SSP_generator")<<"Error: Millislice overlap not defined in SSP DAQ configuration!"<<std::endl;
} catch (...) {}
    throw SSPDAQ::EDAQConfigError("");
  }
  */

  unsigned int useExternalTimestamp=daqConfig.get<unsigned int>("UseExternalTimestamp",2);

  if(useExternalTimestamp>1){
    try{
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Timestamp source not defined, or invalidly defined, in SSP DAQ configuration!"<<std::endl;
    } catch(...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int emptyWriteDelay=daqConfig.get<unsigned int>("EmptyWriteDelay",0);

  if(emptyWriteDelay==0){
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"EmptyWriteDelay not defined in SSP DAQ configuration!"<<std::endl;
    } catch(...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int hardwareClockRate=daqConfig.get<unsigned int>("HardwareClockRate",0);

  if(hardwareClockRate==1){
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Hardware clock rate not defined in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int startOnNOvASync=daqConfig.get<unsigned int>("StartOnNOvASync",2);

  if(startOnNOvASync>1){
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: StartOnNOvASync not defined, or invalidly defined, in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }


  device_interface_->SetMillisliceLength(millisliceLength);
  device_interface_->SetMillisliceOverlap(millisliceOverlap);
  device_interface_->SetUseExternalTimestamp(useExternalTimestamp);
  device_interface_->SetEmptyWriteDelayInus(emptyWriteDelay);
  device_interface_->SetHardwareClockRateInMHz(hardwareClockRate);
  device_interface_->SetStartOnNOvASync(startOnNOvASync);
}


  
void lbne::SSP::start(){
  device_interface_->Start();
  fNNoFragments=0;
  fNFragmentsSent=0;
  fNGetNextCalls=0;
}

void lbne::SSP::stop(){
  device_interface_->Stop();
}

bool lbne::SSP::getNext_(artdaq::FragmentPtrs & frags) {

  if (should_stop()) {
    return false;
  }

  ++fNGetNextCalls;

  bool hasSeenSlice=false;

  unsigned int maxFrags=100;

  for(unsigned int fragsBuilt=0;fragsBuilt<maxFrags;++fragsBuilt){

    std::vector<unsigned int> millislice;

    device_interface_->GetMillislice(millislice);

    if(millislice.size()==0){
      if(!hasSeenSlice){
	++fNNoFragments;
      }
    break;
    }

    hasSeenSlice=true;

    ++fNFragmentsSent;

    if(!(fNFragmentsSent%1000)){
      DAQLogger::LogDebug("SSP_SSP_generator")<<device_interface_->GetIdentifier()
			 <<"Generator sending fragment "<<fNFragmentsSent
			 <<", calls to GetNext "<<fNGetNextCalls
			 <<", of which returned null "<<fNNoFragments<<std::endl;
			 
    }
    
    SSPFragment::Metadata metadata;
    metadata.sliceHeader=*((SSPDAQ::MillisliceHeader*)(void*)millislice.data());

  // And use it, along with the artdaq::Fragment header information
  // (fragment id, sequence id, and user type) to create a fragment

  // We'll use the static factory function 

  // artdaq::Fragment::FragmentBytes(std::size_t payload_size_in_bytes, sequence_id_t sequence_id,
  //  fragment_id_t fragment_id, type_t type, const T & metadata)

  // which will then return a unique_ptr to an artdaq::Fragment
  // object. The advantage of this approach over using the
  // artdaq::Fragment constructor is that, if we were to want to
  // initialize the artdaq::Fragment with a nonzero-size payload (data
  // after the artdaq::Fragment header and metadata), we could provide
  // the size of the payload in bytes, rather than in units of the
  // artdaq::Fragment's RawDataType (8 bytes, as of 3/26/14). The
  // artdaq::Fragment constructor itself was not altered so as to
  // maintain backward compatibility.

    //Too verbose; get rid of this.
    //DAQLogger::LogDebug("SSP_SSP_generator")<<"SSP generator appending event to fragment holder"<<std::endl;
    
    std::size_t dataLength = millislice.size()-SSPDAQ::MillisliceHeader::sizeInUInts;
    
    frags.emplace_back( artdaq::Fragment::FragmentBytes(0,
							ev_counter(), fragment_id(),
							fragment_type_, metadata) );
    
    // Then any overlay-specific quantities next; will need the
    // SSPFragmentWriter class's setter-functions for this
    
    SSPFragmentWriter newfrag(*frags.back());
    
    newfrag.set_hdr_run_number(999);
    
    newfrag.resize(dataLength);
    std::copy(millislice.begin()+SSPDAQ::MillisliceHeader::sizeInUInts,millislice.end(),newfrag.dataBegin());
    
    ev_counter_inc();
  } 
  
  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::SSP) 
