#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <iostream>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  std::string wib_address = ps.get<std::string>("WIB.address");
  bool useWIBFakeData = true;
  (void) useWIBFakeData;
  // JCF, Sep-14-2017

  // It's safer to use one of C++11's "smart" pointers than to use a
  // raw pointer, as objects pointed to by smart pointers will be
  // automatically cleaned up when the smart pointer goes out of
  // scope, without requiring the programmer to cleanup with the
  // "delete" keyword. However, be aware that here, deallocation will
  // occur at the end of the constructor. To make the WIB instance
  // persistent through the lifetime of WIBReader, make it a WIBReader
  // member.

  dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;
  wib = std::make_unique<WIB>( wib_address, "WIB.adt", "FEMB.adt" );

  wib->ResetWIB();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  wib->InitializeDTS(1); // initDTS in script
  std::this_thread::sleep_for(std::chrono::seconds(1));

  for(uint8_t iFEMB=1; iFEMB <= 4; iFEMB++)
  {
    for(uint8_t iCD=1; iCD <= 2; iCD++)
    {
      wib->SetFEMBFakeCOLDATAMode(iFEMB,iCD,0); // 0 for S (counter) or 1 for C (nonsense)
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      wib->SetFEMBStreamSource(iFEMB,iStream,useWIBFakeData);
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      uint64_t sourceByte = 0;
      if(useWIBFakeData) sourceByte = 0xF;
      wib->SourceFEMB(iFEMB,sourceByte);
    }
  }

  for(uint8_t iLink=1; iLink <= 4; iLink++)
  {
    wib->EnableDAQLink_Lite(iLink,true);
  }

  wib->StartSyncDTS();

  wib->Write("FEMB1.DAQ.ENABLE",0xF);
  wib->Write("FEMB2.DAQ.ENABLE",0xF);
  wib->Write("FEMB3.DAQ.ENABLE",0xF);
  wib->Write("FEMB4.DAQ.ENABLE",0xF);

  //fhicl::ParameterSet wib_config = ps.get<fhicl::ParameterSet>("WIB.config");
  //// Read values from FHiCL document and feed them to WIB library
  //for (std::string key : wib_config.get_names()) {
  //  if (wib_config.is_key_to_atom(key)) {
  //    uint32_t value = wib_config.get<long int>(key);
  //    wib->Write(key, value);
  //    dune::DAQLogger::LogInfo("WIBReader") << "Set " << key << " to 0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec;
  //  }
  //}
  dune::DAQLogger::LogInfo("WIBReader") << "Configured WIB";
}

// "shutdown" transition
WIBReader::~WIBReader() {

}

// "start" transition
void WIBReader::start() {

}

// "stop" transition
void WIBReader::stop() {

}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIBReader::getNext_(artdaq::FragmentPtrs& /*frags*/) {
  return false;
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wibdaq::WIBReader)
