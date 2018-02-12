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
  auto wib_table = ps.get<std::string>("WIB.wib_table");
  auto femb_table = ps.get<std::string>("WIB.femb_table");
  auto expected_wib_fw_version = ps.get<unsigned>("WIB.config.expected_wib_fw_version");
  auto expected_femb_fw_version = ps.get<uint32_t>("WIB.config.expected_femb_fw_version");
  auto expected_daq_mode = ps.get<std::string>("WIB.config.expected_daq_mode");
  auto fake_cold_data_modes = ps.get<std::vector<std::vector<unsigned> > >("WIB.config.fake_cold_data_modes"); // 0 SAMPLES, 1 COUNTER
  auto use_WIB_fake_data = ps.get<std::vector<std::vector<bool> > >("WIB.config.use_WIB_fake_data");
  auto enable_DAQ_link = ps.get<std::vector<bool> >("WIB.config.enable_DAQ_link");
  auto enable_DAQ_regs = ps.get<std::vector<unsigned> >("WIB.config.enable_DAQ_regs");

  auto local_clock = ps.get<bool>("WIB.config.local_clock");
  auto PDTS_source = ps.get<unsigned>("WIB.config.PDTS_source"); // 0 back plane, 1 front panel
  auto clock_source = ps.get<unsigned>("WIB.config.clock_source"); // 0 timing system, 1 25Mhz OSC, 2 SMA, 3 Feedback
  auto use_SI5342 = ps.get<bool>("WIB.config.use_SI5342"); // true for FELIX

  auto enable_FEMB = ps.get<std::vector<bool> >("WIB.config.enable_FEMB");
  auto FEMB_configs = ps.get<std::vector<fhicl::ParameterSet> >("WIB.config.FEMBs");

  if (FEMB_configs.size() != 4)
  {
    cet::exception excpt("WIBReader");
    excpt << "Length of WIB.config.FEMBs must be 4, not: " << FEMB_configs.size();
    throw excpt;
  }

  try
  {
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
    dune::DAQLogger::LogDebug("WIBReader") << "N DAQ Links: "  << wib->Read("SYSTEM.DAQ_LINK_COUNT");
    dune::DAQLogger::LogDebug("WIBReader") << "N FEMB Ports: "  << wib->Read("SYSTEM.FEMB_COUNT");
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
  
    if(use_SI5342) // for FELIX
    {
      dune::DAQLogger::LogInfo("WIBReader") << "Configuring and selecting SI5342 for FELIX";
      wib->LoadConfigDAQ_SI5342("default");
      wib->SelectSI5342(1,true);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else
    {
      dune::DAQLogger::LogInfo("WIBReader") << "Not using SI5342 (don't need for RCE)";
    }
  
    if (local_clock)
    {
      dune::DAQLogger::LogInfo("WIBReader") << "Setting up Local Clock";
      wib->LoadConfigDTS_SI5344("default"); // configSI5344 in script
      wib->SelectSI5344(clock_source,1);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      wib->Write("DTS.CONVERT_CONTROL.EN_FAKE",1);
      wib->Write("DTS.CONVERT_CONTROL.LOCAL_TIMESTAMP",1);
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    else // normal timing system
    {
      dune::DAQLogger::LogInfo("WIBReader") << "Setting up DTS";
      wib->InitializeDTS(PDTS_source,clock_source); // initDTS in script
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  
    dune::DAQLogger::LogInfo("WIBReader") << "Writing data source and enabling DAQ links";
  
    for(size_t iFEMB=1; iFEMB <= 4; iFEMB++)
    {
      for(size_t iCD=1; iCD <= 2; iCD++)
      {
        wib->SetFEMBFakeCOLDATAMode(iFEMB,iCD,fake_cold_data_modes.at(iFEMB-1).at(iCD-1)); // 0 for "SAMPLES" (counter) or 1 for "COUNTER" (nonsense)
      }
      for(size_t iStream=1; iStream <= 4; iStream++)
      {
        wib->SetFEMBStreamSource(iFEMB,iStream,!use_WIB_fake_data.at(iFEMB-1).at(iStream-1)); // last arg is bool isReal
      }
      for(size_t iStream=1; iStream <= 4; iStream++)
      {
        uint64_t sourceByte = 0;
        if(use_WIB_fake_data.at(iFEMB-1).at(iStream-1)) sourceByte = 0xF;
        wib->SourceFEMB(iFEMB,sourceByte);
      }
    }

    for(size_t iFEMB=1; iFEMB <= 4; iFEMB++)
    {
      if(enable_FEMB.at(iFEMB-1))
      {
        fhicl::ParameterSet const& FEMB_config = FEMB_configs.at(iFEMB-1);
        auto enable_FEMB_fake_word = FEMB_config.get<bool>("enable_fake_word");
        auto enable_FEMB_fake_waveform = FEMB_config.get<bool>("enable_fake_waveform");

        if(enable_FEMB_fake_word && enable_FEMB_fake_waveform)
        {
          cet::exception excpt("WIBReader");
          excpt << "FEMB"
              << iFEMB
              << " can't both be in fake word and fake waveform mode";
          throw excpt;
        }
        else if(enable_FEMB_fake_word)
        {
          dune::DAQLogger::LogInfo("WIBReader") << "Setting up FEMB"<<iFEMB<<" in fake word mode";
          setupFEMBFakeWord(iFEMB,0xBAF,expected_femb_fw_version);
        }
        else if(enable_FEMB_fake_waveform)
        {
          dune::DAQLogger::LogInfo("WIBReader") << "Setting up FEMB"<<iFEMB<<" in fake waveform mode";
          //setupFEMBFakeWaveform(iFEMB,FEMB_fake_waveforms.at(iFEMB-1),expected_femb_fw_version);
        }
        else
        {
          dune::DAQLogger::LogInfo("WIBReader") << "Setting up FEMB"<<iFEMB;
          setupFEMB(iFEMB,3,3,1,0,0,0,0,0,1,1,1,0,0,0,1,expected_femb_fw_version);
        }
      }
      else
      {
        dune::DAQLogger::LogInfo("WIBReader") << "FEMB"<<iFEMB<<" not enabled";
      }
    }

    wib->Write("SYSTEM.RESET.DAQ_PATH_RESET",1);
    wib->Write("SYSTEM.RESET.DAQ_PATH_RESET",0);
  
    for(size_t iLink=1; iLink <= enable_DAQ_link.size(); iLink++)
    {
      wib->EnableDAQLink_Lite(iLink,enable_DAQ_link.at(iLink-1));
    }
  
    dune::DAQLogger::LogInfo("WIBReader") << "Enabling DTS";
  
    wib->Write("DTS.PDTS_ENABLE",1);
    std::this_thread::sleep_for(std::chrono::seconds(3));
  
    dune::DAQLogger::LogInfo("WIBReader") << "Syncing DTS";
  
    wib->StartSyncDTS();
  
    dune::DAQLogger::LogInfo("WIBReader") << "Enabling DAQ";
  
    for(size_t iFEMB=1; iFEMB <= 4; iFEMB++)
    {
      std::stringstream regNameStream;
      regNameStream << "FEMB" << iFEMB << ".DAQ.ENABLE";
      wib->Write(regNameStream.str(),enable_DAQ_regs.at(iFEMB-1));
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

void WIBReader::setupFEMBFakeWord(size_t iFEMB, unsigned fakeWord, uint32_t expected_femb_fw_version) {
  // Don't forget to disable WIB fake data
  
  wib->FEMBPower(iFEMB,1);
  sleep(1);

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

  // Reset FEMB PLL and transceivers
  wib->WriteFEMB(iFEMB,"TX_PLL_AND_DIGITAL_RST",0x3);
  sleep(1);
  wib->WriteFEMB(iFEMB,"TX_PLL_AND_DIGITAL_RST",0x0);
  sleep(1);

  wib->WriteFEMB(iFEMB,"START_FRAME_MODE_SELECT",0x1);
  sleep(0.01);
  wib->WriteFEMB(iFEMB,"START_FRAME_SWAP",0x1);
  sleep(0.01);

  // Fake Samples
  wib->WriteFEMB(iFEMB,"DATA_TEST_PATTERN",fakeWord);
  sleep(0.01);
  wib->WriteFEMB(iFEMB,"ADC_ASIC_DISABLE",0xFF);
  sleep(0.01);

  //turn on data streaming and adc samples
  wib->WriteFEMB(iFEMB,"STREAM_AND_ADC_DATA_EN",0x9);
  sleep(0.01);
}

void WIBReader::setupFEMBFakeWaveform(size_t iFEMB, std::vector<unsigned> fakeWaveform, uint32_t expected_femb_fw_version) {
  // Don't forget to disable WIB fake data

  if (fakeWaveform.size() != 255)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMBFakeWaveform: FEMB "
        << iFEMB
        << " waveform must be 255 long, but is "
        << fakeWaveform.size()
        << " long";
    throw excpt;
  }

  wib->FEMBPower(iFEMB,1);
  sleep(1);

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

  // Put waveform in FEMB registers
  for (size_t iSample=0; iSample < 255; iSample++)
  {
    wib->WriteFEMB(iFEMB,0x300+iSample,fakeWaveform.at(iSample));
  }

  // Reset FEMB PLL and transceivers
  wib->WriteFEMB(iFEMB,"TX_PLL_AND_DIGITAL_RST",0x3);
  sleep(1);
  wib->WriteFEMB(iFEMB,"TX_PLL_AND_DIGITAL_RST",0x0);
  sleep(1);

  wib->WriteFEMB(iFEMB,"START_FRAME_MODE_SELECT",0x1);
  wib->WriteFEMB(iFEMB,"START_FRAME_SWAP",0x1);

  // Fake Waveform
  wib->WriteFEMB(iFEMB,"ADC_ASIC_DISABLE",0xFF);
  wib->WriteFEMB(iFEMB,"FEMB_TST_MODE",0x1);

  //turn on data streaming and adc samples
  wib->WriteFEMB(iFEMB,"STREAM_AND_ADC_DATA_EN",0x9);
}

void WIBReader::setupFEMB(size_t iFEMB, uint32_t gain, uint32_t shape, uint32_t base, uint32_t leakHigh, uint32_t leak10X, uint32_t acCouple, uint32_t buffer, uint32_t tstIn, uint32_t extClk, uint8_t clk_cs, uint8_t pls_cs, uint8_t dac_sel, uint8_t fpga_dac, uint8_t asic_dac, uint8_t mon_cs, uint32_t expected_femb_fw_version){
  // Don't forget to disable WIB fake data

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
  if (base > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " base should be 0 or 1 is: "
        << base;
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
  if (tstIn > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " tstIn should be 0 or 1 is: "
        << tstIn;
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
  if (clk_cs > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " clk_cs should be 0 or 1 is: "
        << clk_cs;
    throw excpt;
  }
  if (pls_cs > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " pls_cs should be 0 or 1 is: "
        << pls_cs;
    throw excpt;
  }
  if (dac_sel > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " dac_sel should be 0 or 1 is: "
        << dac_sel;
    throw excpt;
  }
  if (fpga_dac > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " fpga_dac should be 0 or 1 is: "
        << fpga_dac;
    throw excpt;
  }
  if (asic_dac > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " asic_dac should be 0 or 1 is: "
        << asic_dac;
    throw excpt;
  }
  if (mon_cs > 1)
  {
    cet::exception excpt("WIBReader");
    excpt << "setupFEMB: FEMB "
        << iFEMB
        << " mon_cs should be 0 or 1 is: "
        << mon_cs;
    throw excpt;
  }

  ///////////////////////////////////////

  wib->FEMBPower(iFEMB,1);
  sleep(1);

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

  wib->WriteFEMB(iFEMB,"REG_RESET",1);
  sleep(1);
  wib->WriteFEMB(iFEMB,"FE_ASIC_RESET",1);
  sleep(1);
  wib->WriteFEMB(iFEMB,"ADC_ASIC_RESET",1);
  sleep(1);

  wib->WriteFEMB(iFEMB,"START_FRAME_MODE_SELECT",0x1);
  wib->WriteFEMB(iFEMB,"START_FRAME_SWAP",0x1);

  std::vector<uint32_t> fe_config = {gain,shape,base,leakHigh,leak10X,acCouple,buffer,tstIn,extClk};

  wib->ConfigFEMB(iFEMB, fe_config, clk_cs, pls_cs, dac_sel, fpga_dac, asic_dac, mon_cs);

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
