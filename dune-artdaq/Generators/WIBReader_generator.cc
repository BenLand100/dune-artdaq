#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

// sends metric of register value at <register name> named WIB.<register name> with <level>
// not averaged
#define sendRegMetric(name,level) artdaq::Globals::metricMan_->sendMetric(name,   (long unsigned int) wib->Read(name), "", level, artdaq::MetricMode::LastPoint, "WIB");
//#define sendRegMetric(name,level) artdaq::Globals::metricMan_->sendMetric(name,   (long unsigned int) wib->Read(name), "", level, false, "WIB");

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  std::string wib_address = ps.get<std::string>("WIB.address");
  std::vector<std::vector<unsigned> > fakeColdDataModes(4,std::vector<unsigned>(2,1));
  std::vector<std::vector<bool> > useWIBFakeData(4,std::vector<bool>(4,true));
  std::vector<bool> enableDAQLink(4,true);
  std::vector<bool> enableDAQRegs(4,0xF);

  dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;
  wib = std::make_unique<WIB>( wib_address, "WIB.adt", "FEMB.adt" );

  wib->ResetWIB();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware Version: 0x" 
        << std::hex << std::setw(8) << std::setfill('0')
        <<  wib->Read("SYSTEM.FW_VERSION")
        << " Synthesized: " 
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_DATE.CENTURY")
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_DATE.YEAR") << "-"
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_DATE.MONTH") << "-"
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_DATE.DAY") << " "
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_TIME.HOUR") << ":"
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_TIME.MINUTE") << ":"
        << std::hex << std::setw(2) << std::setfill('0')
        << wib->Read("SYSTEM.SYNTH_TIME.SECOND");

  dune::DAQLogger::LogInfo("WIBReader") << "Setting up DTS";

  wib->InitializeDTS(1); // initDTS in script
  std::this_thread::sleep_for(std::chrono::seconds(1));

  dune::DAQLogger::LogInfo("WIBReader") << "Writing data source and link registers";

  for(uint8_t iFEMB=1; iFEMB <= 4; iFEMB++)
  {
    for(uint8_t iCD=1; iCD <= 2; iCD++)
    {
      wib->SetFEMBFakeCOLDATAMode(iFEMB,iCD,fakeColdDataModes.at(iFEMB-1).at(iCD-1)); // 0 for "SAMPLES" (counter) or 1 for "COUNTER" (nonsense)
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      wib->SetFEMBStreamSource(iFEMB,iStream,!useWIBFakeData.at(iFEMB-1).at(iStream-1)); // last arg is bool isReal
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

  dune::DAQLogger::LogInfo("WIBReader") << "Enabling DTS";

  wib->Write("DTS.PDTS_ENABLE",1);
  std::this_thread::sleep_for(std::chrono::seconds(3));

  dune::DAQLogger::LogInfo("WIBReader") << "Syncing DTS";

  wib->StartSyncDTS();

  dune::DAQLogger::LogInfo("WIBReader") << "Enabling DAQ";

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
  dune::DAQLogger::LogInfo("WIBReader") <<"getNext_()";
  if(artdaq::Globals::metricMan_ != nullptr) {
    //sendMetric(const string&, const int&, const string&,   int,                bool,    const string&,                         bool)
    //                    name,      value,         units, level, average values=true,        prefix="",   don't apply prefixes=false

    sendRegMetric("SYSTEM.DTS_LOCKED",4);
    sendRegMetric("SYSTEM.EB_LOCKED",4);
    sendRegMetric("SYSTEM.FEMB_LOCKED",4);
    sendRegMetric("SYSTEM.SYS_LOCKED",4);

    sendRegMetric("DTS.CONVERT_CONTROL.STATE",5);
    sendRegMetric("DTS.CONVERT_CONTROL.SYNC_PERIOD",5);
    sendRegMetric("DTS.CONVERT_CONTROL.LOCAL_TIMESTAMP",5);
    sendRegMetric("DTS.PDTS_STATE",5);
    sendRegMetric("DTS.CONVERT_CONTROL.OOS",5);
    sendRegMetric("DTS.EVENT_NUMBER",5);

    sendRegMetric("DAQ_LINK_1.EVENT_COUNT",5);
    sendRegMetric("DAQ_LINK_1.MISMATCH_COUNT",5);
    sendRegMetric("DAQ_LINK_2.EVENT_COUNT",5);
    sendRegMetric("DAQ_LINK_2.MISMATCH_COUNT",5);
    sendRegMetric("DAQ_LINK_3.EVENT_COUNT",5);
    sendRegMetric("DAQ_LINK_3.MISMATCH_COUNT",5);
    sendRegMetric("DAQ_LINK_4.EVENT_COUNT",5);
    sendRegMetric("DAQ_LINK_4.MISMATCH_COUNT",5);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return false;
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wibdaq::WIBReader)
