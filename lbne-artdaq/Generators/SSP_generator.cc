#include "lbne-artdaq/Generators/SSP.hh"
#include "lbne-artdaq/Generators/anlBoard/Log.h"
#include "lbne-artdaq/Generators/anlBoard/anlExceptions.h"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragmentWriter.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

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
  device_interface_=new SSPDAQ::DeviceInterface(interface_type_,0);//board_id_);
  device_interface_->Initialize();
  this->ConfigureDevice(ps);
  this->ConfigureDAQ(ps);
}

void lbne::SSP::ConfigureDevice(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet hardwareConfig( ps.get<fhicl::ParameterSet>("HardwareConfig") );
  std::vector<std::string> hcKeys=hardwareConfig.get_keys();

  for(auto hcIter=hcKeys.begin();hcIter!=hcKeys.end();++hcIter){
    
    //Expect to see a Literals section; parse these literal registers
    if(!hcIter->compare("Literals")){
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
}

void lbne::SSP::ConfigureDAQ(fhicl::ParameterSet const& ps){
  fhicl::ParameterSet daqConfig( ps.get<fhicl::ParameterSet>("DAQConfig") );

  unsigned int millisliceLength=daqConfig.get<unsigned int>("MillisliceLength",0);

  if(millisliceLength==0){
    SSPDAQ::Log::Error()<<"Error: Millislice length not defined in SSP DAQ configuration!"<<std::endl;
    throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int millisliceOverlap=daqConfig.get<unsigned int>("MillisliceOverlap",0);

  if(millisliceOverlap==0){
    SSPDAQ::Log::Error()<<"Error: Millislice overlap not defined in SSP DAQ configuration!"<<std::endl;
    throw SSPDAQ::EDAQConfigError("");
  }

  unsigned int useExternalTimestamp=daqConfig.get<unsigned int>("UseExternalTimestamp",2);

  if(useExternalTimestamp>1){
    SSPDAQ::Log::Error()<<"Error: Timestamp source not defined, or invalidly defined, in SSP DAQ configuration!"<<std::endl;
    throw SSPDAQ::EDAQConfigError("");
  }

  device_interface_->SetMillisliceLength(millisliceLength);
  device_interface_->SetMillisliceOverlap(millisliceOverlap);
  device_interface_->SetUseExternalTimestamp(useExternalTimestamp);
}


  
void lbne::SSP::start(){
  device_interface_->Start();
}

void lbne::SSP::stop(){
  device_interface_->Stop();
}

bool lbne::SSP::getNext_(artdaq::FragmentPtrs & frags) {

  if (should_stop()) {
    return false;
  }

  std::vector<unsigned int> millislice;
  device_interface_->GetMillislice(millislice);

  if(millislice.size()==0){
    return true;
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

  std::cout<<"SSP generator appending event to fragment holder"<<std::endl;

  std::size_t dataLength = (millislice.size()-SSPDAQ::MillisliceHeader::sizeInUInts)*sizeof(unsigned int);

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

  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::SSP) 
