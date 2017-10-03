#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  std::string wib_address = ps.get<std::string>("WIB.address");
  std::vector<std::vector<unsigned> > coldDataModes(4,std::vector<unsigned>(2,0));
  std::vector<std::vector<bool> > useWIBFakeData(4,std::vector<bool>(4,true));
  std::vector<bool> enableDAQLink(4,true);
  std::vector<bool> enableDAQRegs(4,0xF);

  dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;
  wib = std::make_unique<WIB>( wib_address, "WIB.adt", "FEMB.adt" );

  wib->ResetWIB();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  dune::DAQLogger::LogInfo("WIBReader") << "Setting up DTS" <<  wib_address;

  wib->InitializeDTS(1); // initDTS in script
  std::this_thread::sleep_for(std::chrono::seconds(1));

  dune::DAQLogger::LogInfo("WIBReader") << "Writing data source and link registers" <<  wib_address;

  for(uint8_t iFEMB=1; iFEMB <= 4; iFEMB++)
  {
    for(uint8_t iCD=1; iCD <= 2; iCD++)
    {
      wib->SetFEMBFakeCOLDATAMode(iFEMB,iCD,coldDataModes.at(iFEMB-1).at(iCD-1)); // 0 for S (counter) or 1 for C (nonsense)
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      wib->SetFEMBStreamSource(iFEMB,iStream,useWIBFakeData.at(iFEMB-1).at(iStream-1));
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      uint64_t sourceByte = 0;
      if(useWIBFakeData.at(iFEMB-1).at(iStream-1)) sourceByte = 0xF;
      wib->SourceFEMB(iFEMB,sourceByte);
    }
  }

  for(uint8_t iLink=1; iLink <= 4; iLink++)
  {
    wib->EnableDAQLink_Lite(iLink,enableDAQLink.at(iLink-1));
  }

  dune::DAQLogger::LogInfo("WIBReader") << "Syncing DTS" <<  wib_address;

  wib->StartSyncDTS();

  dune::DAQLogger::LogInfo("WIBReader") << "Enableing DAQ" <<  wib_address;

  for(uint8_t iFEMB=1; iFEMB <= 4; iFEMB++)
  {
    std::stringstream regNameStream;
    regNameStream << "FEMB" << iFEMB << ".DAQ.ENABLE";
    wib->Write(regNameStream.get(),enableDAQRegs.at(iFEMB-1));
  }

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
