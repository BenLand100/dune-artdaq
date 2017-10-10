#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "cetlib/exception.h"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

// sends metric of register value at <register name> named WIB.<register name> with <level>
// not averaged
#define sendRegMetric(name,level) artdaq::Globals::metricMan_->sendMetric(name,   (long unsigned int) wib->Read(name), "", level,false, "WIB");

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  auto wib_address = ps.get<std::string>("WIB.address");
  auto wib_table = ps.get<std::string>("WIB.wib_table");
  auto femb_table = ps.get<std::string>("WIB.femb_table");
  auto expected_wib_fw_version = ps.get<unsigned>("WIB.config.expected_wib_fw_version");
  //auto expected_femb_fw_version = ps.get<unsigned>("WIB.config.expected_femb_fw_version");
  auto expected_daq_mode = ps.get<std::string>("WIB.config.expected_daq_mode");
  auto fake_cold_data_modes = ps.get<std::vector<std::vector<unsigned> > >("WIB.config.fake_cold_data_modes");
  auto use_WIB_fake_data = ps.get<std::vector<std::vector<bool> > >("WIB.config.use_WIB_fake_data");
  auto enable_DAQ_link = ps.get<std::vector<bool> >("WIB.config.enable_DAQ_link");
  auto enable_DAQ_regs = ps.get<std::vector<unsigned> >("WIB.config.enable_DAQ_regs");
  auto use_SI5342 = ps.get<bool>("WIB.config.use_SI5342");

  dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;
  wib = std::make_unique<WIB>( wib_address, wib_table, femb_table );

  wib->ResetWIB();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // Check and print firmware version
  uint32_t wib_fw_version = wib->Read("SYSTEM.FW_VERSION");

  dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware Version: 0x" 
        << std::hex << std::setw(8) << std::setfill('0')
        <<  wib_fw_version
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

  if (expected_wib_fw_version != wib_fw_version)
  {
    cet::exception excpt("WIBReader");
    excpt << "WIB Firmware version is "
        << std::hex << std::setw(8) << std::setfill('0')
        << wib_fw_version
        <<" but expect "
        << std::hex << std::setw(8) << std::setfill('0')
        << expected_wib_fw_version
        <<" version in fcl";
    throw excpt;
  }

  // Check if WIB firmware is for RCE or FELIX DAQ
  WIB::WIB_DAQ_t daqMode = wib->GetDAQMode();

  if (daqMode == WIB::RCE)
  {
    dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware setup for RCE DAQ Mode";
    if(expected_daq_mode != "RCE" and expected_daq_mode != "rce")
    {
        cet::exception excpt("WIBReader");
        excpt << "WIB Firmware setup in RCE mode, but expect '"<< expected_daq_mode <<"' mode in fcl";
        throw excpt;
    }
  }
  else if (daqMode == WIB::FELIX)
  {
    dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware setup for FELIX DAQ Mode";
    if(expected_daq_mode != "FELIX" and expected_daq_mode != "felix")
    {
        cet::exception excpt("WIBReader");
        excpt << "WIB Firmware setup in FELIX mode, but expect '"<< expected_daq_mode <<"' mode in fcl";
        throw excpt;
    }
  }
  else
  {
    cet::exception excpt("WIBReader");
    excpt << "Unknown WIB firmware DAQ mode'"<< daqMode << "'";
    throw excpt;
  }

  if(use_SI5342)
  {
    dune::DAQLogger::LogInfo("WIBReader") << "Configuring and selecting SI5342 for FELIX";
    wib->LoadConfigDAQ_SI5342("default");
    wib->SelectSI5342(1,true);
  }
  else
  {
    dune::DAQLogger::LogInfo("WIBReader") << "Not using SI5342 (don't need for RCE)";
  }

  dune::DAQLogger::LogInfo("WIBReader") << "Setting up DTS";

  wib->InitializeDTS(1); // initDTS in script
  std::this_thread::sleep_for(std::chrono::seconds(1));

  dune::DAQLogger::LogInfo("WIBReader") << "Writing data source and link registers";

  for(uint8_t iFEMB=1; iFEMB <= 4; iFEMB++)
  {
    for(uint8_t iCD=1; iCD <= 2; iCD++)
    {
      wib->SetFEMBFakeCOLDATAMode(iFEMB,iCD,fake_cold_data_modes.at(iFEMB-1).at(iCD-1)); // 0 for "SAMPLES" (counter) or 1 for "COUNTER" (nonsense)
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      wib->SetFEMBStreamSource(iFEMB,iStream,!use_WIB_fake_data.at(iFEMB-1).at(iStream-1)); // last arg is bool isReal
    }
    for(uint8_t iStream=1; iStream <= 4; iStream++)
    {
      uint64_t sourceByte = 0;
      if(use_WIB_fake_data.at(iFEMB-1).at(iStream-1)) sourceByte = 0xF;
      wib->SourceFEMB(iFEMB,sourceByte);
    }
  }

  for(uint8_t iLink=1; iLink <= 4; iLink++)
  {
    wib->EnableDAQLink_Lite(iLink,enable_DAQ_link.at(iLink-1));
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
    wib->Write(regNameStream.get(),enable_DAQ_regs.at(iFEMB-1));
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
