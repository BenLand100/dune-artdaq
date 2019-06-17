///////////////////////////////////////////////////////////////////////////////////////////////////
// Class:       PdspChannelMapService
// Module type: service
// File:        PdspChannelMapService.h
// Author:      Jingbo Wang (jiowang@ucdavis.edu), February 2018
//
// Implementation of hardware-offline channel mapping reading from a file.
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PdspChannelMapService.h"

// Bad channel value
unsigned int bad() {
  unsigned int val = std::numeric_limits<unsigned int>::max();
  return val;
}

dune::PdspChannelMapService::PdspChannelMapService(fhicl::ParameterSet const& pset) {

  std::string channelMapFile = pset.get<std::string>("FileName");

  std::string fullname;
  cet::search_path sp("FW_SEARCH_PATH");
  sp.find_file(channelMapFile, fullname);

  if (fullname.empty()) {
    std::cout << "Input file " << channelMapFile << " not found" << std::endl;
    throw cet::exception("File not found");
  }
  else
    std::cout << "PDSP Channel Map: Building TPC wiremap from file " << channelMapFile << std::endl;

  std::ifstream inFile(fullname, std::ios::in);
  std::string line;

  while (std::getline(inFile,line)) {
    unsigned int crateNo, slotNo, fiberNo, FEMBChannel, StreamChannel, slotID, fiberID, chipNo, chipChannel, asicNo, asicChannel, planeType, offlineChannel;
    std::stringstream linestream(line);
    linestream >> crateNo >> slotNo >> fiberNo>> FEMBChannel >> StreamChannel >> slotID >> fiberID >> chipNo >> chipChannel >> asicNo >> asicChannel >> planeType >> offlineChannel;

    // fill lookup tables.  Throw an exception if any number is out of expected bounds.
    // checking for negative values produces compiler warnings as these are unsigned ints

    if (offlineChannel >= fNChans)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Offline Channel: " << offlineChannel << "\n";
      }
    if (crateNo >= fNCrates)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Crate Number: " << crateNo << "\n";
      }
    if (slotNo >= fNSlots)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Slot Number: " << slotNo << "\n";
      }
    if (fiberNo >= fNFibers)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Fiber Number: " << fiberNo << "\n";
      }
    if (StreamChannel >= fNFEMBChans)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood FEMB (Stream) Channel Number: " << StreamChannel << "\n";
      }

    farrayCsfcToOffline[crateNo][slotNo][fiberNo][StreamChannel] = offlineChannel;
    fvAPAMap[offlineChannel] = crateNo; 
    fvWIBMap[offlineChannel] = slotNo; 
    fvFEMBMap[offlineChannel] = fiberNo; 
    fvFEMBChannelMap[offlineChannel] = FEMBChannel; 
    fvStreamChannelMap[offlineChannel] = StreamChannel;
    fvSlotIdMap[offlineChannel] = slotID; 
    fvFiberIdMap[offlineChannel] = fiberID; 
    fvChipMap[offlineChannel] = chipNo; 
    fvChipChannelMap[offlineChannel] = chipChannel;
    fvASICMap[offlineChannel] = asicNo;
    fvASICChannelMap[offlineChannel] = asicChannel;
    fvPlaneMap[offlineChannel] = planeType;

  }
  inFile.close();


  std::string SSPchannelMapFile = pset.get<std::string>("SSPFileName");

  std::string SSPfullname;
  sp.find_file(SSPchannelMapFile, SSPfullname);

  if (SSPfullname.empty()) {
    std::cout << "Input file for SSP Channel Map " << SSPchannelMapFile << " not found in FW_SEARCH_PATH " << std::endl;
    throw cet::exception("File not found");
  }
  else
    std::cout << "PDSP Channel Map: Building SSP channel map from file " << SSPchannelMapFile << std::endl;

  std::ifstream SSPinFile(SSPfullname, std::ios::in);

  while (std::getline(SSPinFile,line)) {
    unsigned int onlineChannel, APA, SSP, SSPGlobal, ChanWithinSSP, SSPModule, offlineChannel;
    std::stringstream linestream(line);
    linestream >> onlineChannel >> APA >> SSP >> SSPGlobal >> ChanWithinSSP >> SSPModule >> offlineChannel;

    // fill lookup tables.  Throw an exception if any number is out of expected bounds.
    // checking for negative values produces compiler warnings as these are unsigned ints

    if (onlineChannel >= fNSSPChans)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood SSP Online Channel: " << onlineChannel << "\n";
      }
    if (offlineChannel >= fNSSPChans)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood SSP Offline Channel: " << offlineChannel << "\n";
      }
    if (APA >= fNAPAs)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood APA Number in SSP map file: " << APA << "\n";
      }
    if (SSP >= fNSSPsPerAPA)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood SSP number within this APA: " << SSP << " " << APA << "\n";
      }
    if (SSPGlobal >= fNSSPs)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Global SSP number: " << SSPGlobal << "\n";
      }
    if (ChanWithinSSP >= fNChansPerSSP)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood Channel within SSP Number: " << ChanWithinSSP << " " << SSPGlobal << "\n";
      }
    if (SSPModule >= 10)
      {
	throw cet::exception("PdspChannelMapService") << "Ununderstood SSP Module Number: " << SSPModule << "\n";
      }

    farraySSPOnlineToOffline[onlineChannel] = offlineChannel;
    farraySSPOfflineToOnline[offlineChannel] = onlineChannel;
    fvSSPAPAMap[offlineChannel] = APA;
    fvSSPWithinAPAMap[offlineChannel] = SSP;
    fvSSPGlobalMap[offlineChannel] = SSPGlobal;
    fvSSPChanWithinSSPMap[offlineChannel] = ChanWithinSSP;
    fvSSPModuleMap[offlineChannel] = SSPModule;
  }
  SSPinFile.close();

  fHaveWarnedAboutBadCrateNumber = false;
  fHaveWarnedAboutBadSlotNumber = false;
  fHaveWarnedAboutBadFiberNumber = false;
  fSSPHaveWarnedAboutBadOnlineChannelNumber = false;
}

dune::PdspChannelMapService::PdspChannelMapService(fhicl::ParameterSet const& pset, art::ActivityRegistry&) : PdspChannelMapService(pset) {
}

unsigned int dune::PdspChannelMapService::GetOfflineNumberFromDetectorElements(unsigned int crate, unsigned int slot, unsigned int fiber, unsigned int streamchannel) {

  unsigned int offlineChannel;
  unsigned int lcrate = crate;
  unsigned int lslot = slot;
  unsigned int lfiber = fiber;

  if (crate >= fNCrates)
    {
      if (!fHaveWarnedAboutBadCrateNumber)
	{
	  mf::LogWarning("PdspChannelMapService: Crate Number too high, using crate number zero as a fallback.  Ununderstood crate number: ") << crate;
	  fHaveWarnedAboutBadCrateNumber = true;
	}
      lcrate = 0;
    }

  if (slot >= fNSlots)
    {
      if (!fHaveWarnedAboutBadSlotNumber)
	{
	  mf::LogWarning("PdspChannelMapService: Slot Number too high, using slot number zero as a fallback.  Ununderstood slot number: ") << slot;
	  fHaveWarnedAboutBadSlotNumber = true;
	}
      lslot = 0;
    }

  if (fiber >= fNFibers)
    {
      if (!fHaveWarnedAboutBadFiberNumber)
	{
	  mf::LogWarning("PdspChannelMapService: Fiber Number too high, using fiber number zero as a fallback.  Ununderstood fiber number: ") << fiber;
	  fHaveWarnedAboutBadFiberNumber = true;
	}
      lfiber = 0;
    }

  if (streamchannel >= fNFEMBChans)
    {
      throw cet::exception("PdspChannelMapService") << "Ununderstood Stream (FEMB) chan: " 
						    << crate << " " << slot << " " << fiber << " " << streamchannel << "\n";
    }

  offlineChannel = farrayCsfcToOffline[lcrate][lslot][lfiber][streamchannel];
  return offlineChannel;
}

unsigned int dune::PdspChannelMapService::APAFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvAPAMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::WIBFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvWIBMap[offlineChannel];

}

unsigned int dune::PdspChannelMapService::FEMBFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvFEMBMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::FEMBChannelFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvFEMBChannelMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::StreamChannelFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvStreamChannelMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SlotIdFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvSlotIdMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::FiberIdFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvFiberIdMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::ChipFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvChipMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::ChipChannelFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvChipChannelMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::ASICFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvASICMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::ASICChannelFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvASICChannelMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::PlaneFromOfflineChannel(unsigned int offlineChannel) const {
  check_offline_channel(offlineChannel);
  return fvFEMBMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPOfflineChannelFromOnlineChannel(unsigned int onlineChannel) 
{
  unsigned int lchannel = onlineChannel;

  if (onlineChannel > fNSSPChans)
    {
      if (!fSSPHaveWarnedAboutBadOnlineChannelNumber)
	{
	  mf::LogWarning("PdspChannelMapService: Online Channel Number too high, using zero as a fallback: ") << onlineChannel;
	  fSSPHaveWarnedAboutBadOnlineChannelNumber = true;
	}
      lchannel = 0;
    }
  return farraySSPOnlineToOffline[lchannel];
}

unsigned int dune::PdspChannelMapService::SSPOnlineChannelFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return farraySSPOfflineToOnline[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPAPAFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return fvSSPAPAMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPWithinAPAFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return fvSSPWithinAPAMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPGlobalFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return fvSSPGlobalMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPChanWithinSSPFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return fvSSPChanWithinSSPMap[offlineChannel];
}

unsigned int dune::PdspChannelMapService::SSPModuleFromOfflineChannel(unsigned int offlineChannel) const
{
  SSP_check_offline_channel(offlineChannel);
  return fvSSPModuleMap[offlineChannel];
}

DEFINE_ART_SERVICE(dune::PdspChannelMapService)
