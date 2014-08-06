#include "lbne-artdaq/Generators/SSP.hh"

#include "art/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "cetlib/exception.h"
#include "lbne-artdaq/Overlays/SSPFragment.hh"
#include "lbne-artdaq/Overlays/SSPFragmentWriter.hh"
#include "lbne-artdaq/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "artdaq/Utilities/SimpleLookupPolicy.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>

#include <unistd.h>

namespace {

  size_t typeToADC(lbne::detail::FragmentType type)
  {
    switch (type) {
    case lbne::detail::FragmentType::PHOTON:
      return 8*sizeof(unsigned int);
      break;
    default:
      throw art::Exception(art::errors::Configuration)
        << "Unknown board type "
        << type
        << " ("
        << lbne::fragmentTypeToString(type)
        << ").\n";
    };
  }

}



lbne::SSP::SSP(fhicl::ParameterSet const & ps)
  :
  CommandableFragmentGenerator(ps),
  fragment_type_(lbne::detail::PHOTON),
  board_id_(ps.get<unsigned int>("board_id",0)),
  interface_type_(ps.get<unsigned int>("interface_type",0) ?
		  SSPDAQ::DeviceManager::kUSB : SSPDAQ::DeviceManager::kEthernet)
{

  device_interface_=new SSPDAQ::DeviceInterface(interface_type_,board_id_);
  device_interface_->Initialize();
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

  SSPDAQ::EventPacket eventPacket;
  device_interface_->GetEvent(eventPacket);

  SSPFragment::Metadata metadata;
  metadata.daqHeader=eventPacket.header;

  std::cout<<"Header: "<<metadata.daqHeader.header<<std::endl;				// 0xAAAAAAAA
  std::cout<<"triggerID: "<<metadata.daqHeader.triggerID<<std::endl;				// 0xAAAAAAAA
  std::cout<<"timestamp: "<<metadata.daqHeader.timestamp[1]<<std::endl;				// 0xAAAAAAAA
  std::cout<<"length: "<<metadata.daqHeader.length<<std::endl;				// 0xAAAAAAAA


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

  std::size_t initial_payload_size = 0;

  frags.emplace_back( artdaq::Fragment::FragmentBytes(initial_payload_size,
						      ev_counter(), fragment_id(),
						      fragment_type_, metadata) );

  // Then any overlay-specific quantities next; will need the
  // SSPFragmentWriter class's setter-functions for this

  SSPFragmentWriter newfrag(*frags.back());

  newfrag.set_hdr_run_number(999);

  unsigned int dataLength = metadata.daqHeader.length-sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
  
  newfrag.resize(dataLength);

  std::cout<<"dataLength (uint) calculated as "<<dataLength<<std::endl;
  std::cout<<"header size(bytes) is "<<sizeof(SSPDAQ::EventHeader)<<std::endl;

  device_interface_->GetEvent(eventPacket);
  std::copy(eventPacket.data.begin(),eventPacket.data.end(),newfrag.dataBegin());

  // And generate nADCcounts ADC values ranging from 0 to max with an
  // equal probability over the full range (a specific and perhaps
  // not-too-physical example of how one could generate simulated
  // data)

  /*std::generate_n(newfrag.dataBegin(), nADCcounts_,
  		  [&]() {
  		    return static_cast<SSPFragment::adc_t>
  		      ((*uniform_distn_)( engine_ ));
  		  }
  		  );
  */
  // Check and make sure that no ADC values in this fragment are
  // larger than the max allowed

  //newfrag.fastVerify( metadata.num_adc_bits );

  ev_counter_inc();

  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::SSP) 
