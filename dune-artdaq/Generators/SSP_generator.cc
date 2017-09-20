#include "dune-artdaq/Generators/SSP.hh"
#include "dune-artdaq/Generators/anlBoard/anlExceptions.h"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "canvas/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "dune-raw-data/Overlays/SSPFragmentWriter.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.hh"
#include <boost/lexical_cast.hpp>

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>

dune::SSP::SSP(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  fragment_type_(dune::detail::PHOTON),
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

void dune::SSP::ConfigureDevice(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet hardwareConfig( ps.get<fhicl::ParameterSet>("HardwareConfig") );
  std::vector<std::string> hcKeys=hardwareConfig.get_names();

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
      std::vector<std::string> lrKeys=literalRegisters.get_names();

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

void dune::SSP::BuildChannelControlRegisters(fhicl::ParameterSet const& ps,std::vector<unsigned int>& reg){
  std::vector<std::string> ccKeys=ps.get_names();

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

void dune::SSP::ConfigureDAQ(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet daqConfig( ps.get<fhicl::ParameterSet>("DAQConfig") );

  unsigned int preTrigLength=daqConfig.get<unsigned int>("PreTrigLength",0);

  if(preTrigLength==0){
    
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Pretrigger sample length not defined in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int postTrigLength=daqConfig.get<unsigned int>("PostTrigLength",0);

  if(postTrigLength==0){
    
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Posttrigger sample length not defined in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int useExternalTimestamp=daqConfig.get<unsigned int>("UseExternalTimestamp",2);

  if(useExternalTimestamp>1){
    try{
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Timestamp source not defined, or invalidly defined, in SSP DAQ configuration!"<<std::endl;
    } catch(...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int triggerWriteDelay=daqConfig.get<unsigned int>("TriggerWriteDelay",0);

  if(triggerWriteDelay==0){
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"TriggerWriteDelay not defined in SSP DAQ configuration!"<<std::endl;
    } catch(...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  int dummyPeriod=daqConfig.get<int>("DummyTriggerPeriod",-1);

  unsigned int hardwareClockRate=daqConfig.get<unsigned int>("HardwareClockRate",0);

  if(hardwareClockRate==1){
    try {
      DAQLogger::LogError("SSP_SSP_generator")<<"Error: Hardware clock rate not defined in SSP DAQ configuration!"<<std::endl;
    } catch (...) {}
      throw SSPDAQ::EDAQConfigError("");
  }

  device_interface_->SetPreTrigLength(preTrigLength);
  device_interface_->SetPostTrigLength(postTrigLength);
  device_interface_->SetUseExternalTimestamp(useExternalTimestamp);
  device_interface_->SetTriggerWriteDelay(triggerWriteDelay);
  device_interface_->SetDummyPeriod(dummyPeriod);
  device_interface_->SetHardwareClockRateInMHz(hardwareClockRate);
}


  
void dune::SSP::start(){
  usleep(10000000);
  device_interface_->Start();
  fNNoFragments=0;
  fNFragmentsSent=0;
  fNGetNextCalls=0;
}

void dune::SSP::stop(){
  device_interface_->Stop();
}

bool dune::SSP::getNext_(artdaq::FragmentPtrs & frags) {

  if (should_stop()) {
    return false;
  }

  ++fNGetNextCalls;

  bool hasSeenSlice=false;

  unsigned int maxFrags=100;

  for(unsigned int fragsBuilt=0;fragsBuilt<maxFrags;++fragsBuilt){

    std::vector<unsigned int> millislice;

    // JCF, Mar-8-2016

    // Could I just wrap this in a try-catch block?

    device_interface_->ReadEvents(millislice);

    if (device_interface_->exception())
      {
	set_exception(true);
	DAQLogger::LogError("SSP") << "dune::SSP::getNext_ : found device interface thread in exception state";
      }

    static size_t ncalls = 1;
    static size_t ncalls_with_millislice = 0;

    if (millislice.size() > 0) {
      ncalls_with_millislice++;
    }

    if (ncalls % 100 == 0) {
      DAQLogger::LogDebug("SSP_SSP_generator") << "On call #" << ncalls << ", there have been " << ncalls_with_millislice << " calls where the millislice size was greater than zero";
    }

    ncalls++;

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

  // We'll use the static factory function 
  // artdaq::Fragment::FragmentBytes(std::size_t payload_size_in_bytes, sequence_id_t sequence_id,
  //  fragment_id_t fragment_id, type_t type, const T & metadata)
  // which will then return a unique_ptr to an artdaq::Fragment
  // object.

    std::size_t dataLength = millislice.size()-SSPDAQ::MillisliceHeader::sizeInUInts;
    
    frags.emplace_back( artdaq::Fragment::FragmentBytes(0,
							ev_counter(), fragment_id(),
							fragment_type_, metadata) );

    auto timestamp = (metadata.sliceHeader.triggerTime + 1057) / 3 ;


    DAQLogger::LogInfo("SSP_SSP_generator") << "SSP fragment w/ fragment ID " << frags.back()->fragmentID() << " timestamp is " << timestamp;
      
    frags.back()->setTimestamp(timestamp);   
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
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::SSP) 
