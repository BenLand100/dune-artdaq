#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "cetlib/exception.h"

#include "BUException/ExceptionBase.hh"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

// sends metric of register value at <register name> named WIB.<register name> with <level>
// not averaged or summed, just the last value
#define sendRegMetric(name,level) artdaq::Globals::metricMan_->sendMetric(name,   (long unsigned int) wib->Read(name), "", level, artdaq::MetricMode::LastPoint, "WIB");

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  auto wib_address = ps.get<std::string>("WIB.address");

  auto wib_table = ps.get<std::string>("WIB.config.wib_table");
  auto femb_table = ps.get<std::string>("WIB.config.femb_table");

  auto expected_wib_fw_version = ps.get<unsigned>("WIB.config.expected_wib_fw_version");
  auto expected_daq_mode = ps.get<std::string>("WIB.config.expected_daq_mode");

  auto use_WIB_fake_data = ps.get<std::vector<bool> >("WIB.config.use_WIB_fake_data");
  auto use_WIB_fake_data_counter = ps.get<bool>("WIB.config.use_WIB_fake_data_counter"); // false SAMPLES, true COUNTER

  auto local_clock = ps.get<bool>("WIB.config.local_clock"); // use local clock if true, else DTS
  auto DTS_source = ps.get<unsigned>("WIB.config.DTS_source"); // 0 back plane, 1 front panel
  auto partition_number = ps.get<unsigned>("WIB.config.partition_number"); // partition or timing group number

  // If these are true, will continue on error, if false, will raise an exception
  auto continueOnFEMBRegReadError = ps.get<bool>("WIB.config.continueOnError.FEMBRegReadError");
  auto continueOnFEMBSPIError = ps.get<bool>("WIB.config.continueOnError.FEMBSPIError");
  auto continueOnFEMBSyncError = ps.get<bool>("WIB.config.continueOnError.FEMBSyncError");
  auto continueIfListOfFEMBClockPhasesDontSync = ps.get<bool>("WIB.config.continueOnError.ListOfFEMBClockPhasesDontSync");

  auto enable_FEMBs = ps.get<std::vector<bool> >("WIB.config.enable_FEMBs");
  auto FEMB_configs = ps.get<std::vector<fhicl::ParameterSet> >("WIB.config.FEMBs");

  if (use_WIB_fake_data.size() != 4)
  {
    cet::exception excpt("WIBReader");
    excpt << "Length of WIB.config.use_WIB_fake_data must be 4, not: " << use_WIB_fake_data.size();
    throw excpt;
  }

  if (FEMB_configs.size() != 4)
  {
    cet::exception excpt("WIBReader");
    excpt << "Length of WIB.config.FEMBs must be 4, not: " << FEMB_configs.size();
    throw excpt;
  }

  if(partition_number > 15)
  {
    cet::exception excpt("WIBReader");
    excpt << "partition_number must be 0-15, not: " << partition_number;
    throw excpt;
  }

  try
  {
    dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;
    wib = std::make_unique<WIB>( wib_address, wib_table, femb_table );

    // Set DIM do-not-disturb
    wib->Write("SYSTEM.SLOW_CONTROL_DND",1);

    // Set whether to continue on errors
    wib->SetContinueOnFEMBRegReadError(continueOnFEMBRegReadError);
    wib->SetContinueOnFEMBSPIError(continueOnFEMBSPIError);
    wib->SetContinueOnFEMBSyncError(continueOnFEMBSyncError);
    wib->SetContinueIfListOfFEMBClockPhasesDontSync(continueIfListOfFEMBClockPhasesDontSync);
  
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
    dune::DAQLogger::LogDebug("WIBReader") << "N DAQ Links: "  << wib->Read("SYSTEM.DAQ_LINK_COUNT");
    dune::DAQLogger::LogDebug("WIBReader") << "N FEMB Ports: "  << wib->Read("SYSTEM.FEMB_COUNT");
    WIB::WIB_DAQ_t daqMode = wib->GetDAQMode();
  
    if (daqMode == WIB::RCE)
    {
      dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware setup for RCE DAQ Mode";
      if(expected_daq_mode != "RCE" &&
         expected_daq_mode != "rce" && 
         expected_daq_mode != "ANY" && 
         expected_daq_mode != "any"
        )
      {
          cet::exception excpt("WIBReader");
          excpt << "WIB Firmware setup in RCE mode, but expect '"<< expected_daq_mode <<"' mode in fcl";
          throw excpt;
      }
    }
    else if (daqMode == WIB::FELIX)
    {
      dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware setup for FELIX DAQ Mode";
      if(expected_daq_mode != "FELIX" && 
         expected_daq_mode != "felix" &&
         expected_daq_mode != "ANY" &&
         expected_daq_mode != "any"
        )
      {
          cet::exception excpt("WIBReader");
          excpt << "WIB Firmware setup in FELIX mode, but expect '"<< expected_daq_mode <<"' mode in fcl";
          throw excpt;
      }
    }
    else if (daqMode == WIB::UNKNOWN)
    {
      cet::exception excpt("WIBReader");
      excpt << "WIB Firmware DAQ setup UNKNOWN";
      throw excpt;
      //dune::DAQLogger::LogInfo("WIBReader") << "WIB Firmware DAQ setup UNKNOWN";
    }
    else
    {
      cet::exception excpt("WIBReader");
      excpt << "Bogus WIB firmware DAQ mode "<< ((unsigned) daqMode);
      throw excpt;
      //dune::DAQLogger::LogInfo("WIBReader") << "Bogus WIB firmware DAQ mode "<< ((unsigned) daqMode);
    }

    // Reset and setup clock
    wib->ResetWIBAndCfgDTS(local_clock,partition_number,DTS_source);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  
    // Configure WIB fake data enable and mode
    dune::DAQLogger::LogInfo("WIBReader") << "Configuring WIB Fake Data";
    dune::DAQLogger::LogInfo("WIBReader") << "Is Fake:"
                                          << " FEMB1: " << use_WIB_fake_data.at(0)
                                          << " FEMB2: " << use_WIB_fake_data.at(1)
                                          << " FEMB3: " << use_WIB_fake_data.at(2)
                                          << " FEMB4: " << use_WIB_fake_data.at(3);
    wib->ConfigWIBFakeData(use_WIB_fake_data.at(0),
                           use_WIB_fake_data.at(1),
                           use_WIB_fake_data.at(2),
                           use_WIB_fake_data.at(3),
                           use_WIB_fake_data_counter);
  
    // Configure FEMBs
    for(size_t iFEMB=1; iFEMB <= 4; iFEMB++)
    {
      if(enable_FEMBs.at(iFEMB-1))
      {
        fhicl::ParameterSet const& FEMB_config = FEMB_configs.at(iFEMB-1);
        auto enable_FEMB_fake_data = FEMB_config.get<bool>("enable_fake_data");

        if(enable_FEMB_fake_data)
        {
          dune::DAQLogger::LogInfo("WIBReader") << "Setting up FEMB "<<iFEMB<<" for fake data";
          setupFEMBFakeData(iFEMB,FEMB_config,continueOnFEMBRegReadError);
        }
        else
        {
          dune::DAQLogger::LogInfo("WIBReader") << "Setting up FEMB"<<iFEMB;
          setupFEMB(iFEMB,FEMB_config,continueOnFEMBRegReadError);
        }
      }
      else
      {
        dune::DAQLogger::LogInfo("WIBReader") << "FEMB"<<iFEMB<<" not enabled";
      }
    }

    dune::DAQLogger::LogInfo("WIBReader") << "Enabling DAQ links";
    wib->StartStreamToDAQ();
  
    // Un-set DIM do-not-disturb
    wib->Write("SYSTEM.SLOW_CONTROL_DND",0);

  } // try
  catch (const BUException::exBase& exc)
  {
    cet::exception excpt("WIBReader");
    excpt << "Unhandled BUException: "
        << exc.what()
        << ": "
        << exc.Description();
    throw excpt;
  }
  dune::DAQLogger::LogInfo("WIBReader") << "Configured WIB";
}

void WIBReader::setupFEMBFakeData(size_t iFEMB, fhicl::ParameterSet const& FEMB_config, bool continueOnFEMBRegReadError) {
  // Don't forget to disable WIB fake data
  
  wib->FEMBPower(iFEMB,1);
  sleep(1);

  if(wib->ReadFEMB(iFEMB,"VERSION_ID") == wib->ReadFEMB(iFEMB,"SYS_RESET")) { // can't read register if equal
    if(continueOnFEMBRegReadError){
      dune::DAQLogger::LogWarning("WIBReader") << "Warning: Can't read registers from FEMB " << int(iFEMB) << ". Powering it down and continuing on to others";
      wib->FEMBPower(iFEMB,0);
      return;
    }
    else
    {
      wib->FEMBPower(iFEMB,0);
      cet::exception excpt("WIBReader");
      excpt << "Can't read registers from FEMB " << int(iFEMB);
      throw excpt;
    }
  }

  auto expected_femb_fw_version = FEMB_config.get<uint32_t>("expected_femb_fw_version");
  uint32_t femb_fw_version = wib->ReadFEMB(iFEMB,"VERSION_ID");
  if (expected_femb_fw_version != femb_fw_version)
  {
    cet::exception excpt("WIBReader");
    excpt << "FEMB" << iFEMB << " Firmware version is "
        << std::hex << std::setw(8) << std::setfill('0')
        << femb_fw_version
        <<" but expect "
        << std::hex << std::setw(8) << std::setfill('0')
        << expected_femb_fw_version
        <<" version in fcl";
    throw excpt;
  }

  uint8_t fake_mode = 0;
  uint16_t fake_word = 0;
  uint8_t femb_number = iFEMB;
  std::vector<uint32_t> fake_waveform;

  auto fakeDataSelect = FEMB_config.get<std::string>("fake_data_select");
  if (fakeDataSelect == "fake_word")
  {
    fake_mode = 1;
    fake_word = FEMB_config.get<uint32_t>("fake_word");
  }
  else if (fakeDataSelect == "fake_waveform")
  {
    fake_mode = 2;
    fake_waveform = FEMB_config.get<std::vector<uint32_t> >("fake_waveform");
    if (fake_waveform.size() != 255)
    {
      cet::exception excpt("WIBReader");
      excpt << "setupFEMBFakeData: FEMB "
          << iFEMB
          << " fake_waveform must be 255 long, but is "
          << fake_waveform.size()
          << " long";
      throw excpt;
    }
  }
  else if (fakeDataSelect == "femb_channel_id")
  {
    fake_mode = 3;
  }
  else if (fakeDataSelect == "counter_channel_id")
  {
    fake_mode = 4;
  }
  else
  {
    cet::exception excpt("WIBReader");
    excpt << "FEMB" << iFEMB << " fake_data_select is \""
        << fakeDataSelect
        <<"\" but expect "
        <<" fake_word, fake_waveform, femb_channel_id, or counter_channel_id";
    throw excpt;
  }

  wib->ConfigFEMBFakeData(iFEMB,fake_mode,fake_word,femb_number,fake_waveform);
}

void WIBReader::setupFEMB(size_t iFEMB, fhicl::ParameterSet const& FEMB_config, bool continueOnFEMBRegReadError){
  // Don't forget to disable WIB fake data

  const auto gain = FEMB_config.get<uint32_t>("gain");
  const auto shape = FEMB_config.get<uint32_t>("shape");
  const auto baselineHigh = FEMB_config.get<uint32_t>("baselineHigh");
  const auto leakHigh = FEMB_config.get<uint32_t>("leakHigh");
  const auto leak10X = FEMB_config.get<uint32_t>("leak10X");
  const auto acCouple = FEMB_config.get<uint32_t>("acCouple");
  const auto buffer = FEMB_config.get<uint32_t>("buffer");
  const auto extClk = FEMB_config.get<uint32_t>("extClk");

  const auto clk_phases = FEMB_config.get<std::vector<uint16_t> >("clk_phases");
  const auto pls_mode = FEMB_config.get<uint32_t>("pls_mode");
  const auto pls_dac_val = FEMB_config.get<uint32_t>("pls_dac_val");

  const auto start_frame_mode_sel = FEMB_config.get<uint32_t>("start_frame_mode_sel");
  const auto start_frame_swap = FEMB_config.get<uint32_t>("start_frame_swap");

  const auto expected_femb_fw_version = FEMB_config.get<uint32_t>("expected_femb_fw_version");

  //////////////////////

  if (gain > 3)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " gain shouldn't be larger than 3 is: "
        << gain;
    throw excpt;
  }
  if (shape > 3)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " shape shouldn't be larger than 3 is: "
        << shape;
    throw excpt;
  }
  if (baselineHigh > 2)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " baselineHigh should be 0 or 1 or 2 is: "
        << baselineHigh;
    throw excpt;
  }
  if (leakHigh > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " leakHigh should be 0 or 1 is: "
        << leakHigh;
    throw excpt;
  }
  if (leak10X > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " leak10X should be 0 or 1 is: "
        << leak10X;
    throw excpt;
  }
  if (acCouple > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " acCouple should be 0 or 1 is: "
        << acCouple;
    throw excpt;
  }
  if (buffer > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " buffer should be 0 or 1 is: "
        << buffer;
    throw excpt;
  }
  if (extClk > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " extClk should be 0 or 1 is: "
        << extClk;
    throw excpt;
  }
  if (clk_phases.size() == 0)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " clk_phases size should be > 0 ";
    throw excpt;
  }
  if (pls_mode > 2)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " pls_mode should be 0 (off) 1 (FE ASIC internal) or 2 (FPGA external) is: "
        << pls_mode;
    throw excpt;
  }
  if ((pls_mode == 1 && pls_dac_val > 63) || (pls_mode == 2 && pls_dac_val > 31))
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " pls_dac_val should be 0-31 in pls_mode 1 or 0-63 in pls_mode 2."
        << " pls_mode is " << pls_mode
        << " pls_dac_val is " << pls_dac_val;
    throw excpt;
  }
  if (start_frame_mode_sel > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " start_frame_mode_sel should be 0 or 1 is: "
        << start_frame_mode_sel;
    throw excpt;
  }
  if (start_frame_swap > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " start_frame_swap should be 0 or 1 is: "
        << start_frame_swap;
    throw excpt;
  }

  ///////////////////////////////////////

  wib->FEMBPower(iFEMB,1);
  sleep(1);

  if(wib->ReadFEMB(iFEMB,"VERSION_ID") == wib->ReadFEMB(iFEMB,"SYS_RESET")) { // can't read register if equal
    if(continueOnFEMBRegReadError){
      dune::DAQLogger::LogWarning("WIBReader") << "Warning: Can't read registers from FEMB " << int(iFEMB) << ". Powering it down and continuing on to others";
      wib->FEMBPower(iFEMB,0);
      return;
    }
    else
    {
      wib->FEMBPower(iFEMB,0);
      cet::exception excpt("WIBReader");
      excpt << "Can't read registers from FEMB " << int(iFEMB);
      throw excpt;
    }
  }

  uint32_t femb_fw_version = wib->ReadFEMB(iFEMB,"VERSION_ID");
  if (expected_femb_fw_version != femb_fw_version)
  {
    cet::exception excpt("WIBReader");
    excpt << "FEMB" << iFEMB << " Firmware version is "
        << std::hex << std::setw(8) << std::setfill('0')
        << femb_fw_version
        <<" but expect "
        << std::hex << std::setw(8) << std::setfill('0')
        << expected_femb_fw_version
        <<" version in fcl";
    throw excpt;
  }

  std::vector<uint32_t> fe_config = {gain,shape,baselineHigh,leakHigh,leak10X,acCouple,buffer,extClk};

  wib->ConfigFEMB(iFEMB, fe_config, clk_phases, pls_mode, pls_dac_val, start_frame_mode_sel, start_frame_swap);

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
  //dune::DAQLogger::LogInfo("WIBReader") <<"getNext_()";
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
