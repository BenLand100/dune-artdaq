////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: MonitoringData
// File: MonitoringData.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class to handle the creation of monitoring data.
// Owns all the data containers and has methods for filling with online data and for writing and
// saving the output in relevant places.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MonitoringData.hxx"

void OnlineMonitoring::MonitoringData::BeginMonitoring(int run, int subrun) {

  /// Sets up monitoring for new subrun

  // Set up new subrun
  fHistArray.Clear();
  fCanvas = new TCanvas("canv","",800,600);
  //receivedData = false; checkedFileSizes = false; _interestingchannelsfilled = false;
  checkedFileSizes = false;

  // Get directory for this run
  std::ostringstream directory;
  directory << HistSavePath << "Run" << run << "Subrun" << subrun << "/";
  HistSaveDirectory = directory.str();

  // Make the directory to save the files
  std::ostringstream cmd;
  cmd << "touch " << directory.str() << "; rm -rf " << directory.str() << "; mkdir " << directory.str();
  system(cmd.str().c_str());

  // Outfile
  fDataFile = new TFile(HistSaveDirectory+TString("monitoringRun"+std::to_string(run)+"Subrun"+std::to_string(subrun)+".root"),"RECREATE");

  // Tree
  if (fMakeTree) {
    fDataTree = new TTree("RawData","Raw Data");
    fDataTree->Branch("RCE",&fRCEADC);
    fDataTree->Branch("SSP",&fSSPADC);
  }

  // Define all histograms
  // Nomenclature: Name is used to save the histogram, Title has format : histogramTitle_histDrawOptions_canvasOptions;x-title;y-title;z-title

  // RCE hists
  hADCMeanChannel                       = new TProfile("ADCMeanChannel","RCE ADC Mean_\"histl\"_none;Channel;RCE ADC Mean",NRCEChannels,0,NRCEChannels);
  hADCRMSChannel                        = new TProfile("ADCRMSChannel","RCE ADC RMS_\"histl\"_none;Channel;RCE ADC RMS",NRCEChannels,0,NRCEChannels);
  hAvADCChannelEvent                    = new TH2D("AvADCChannelEvent","Average RCE ADC Value_\"colz\"_none;Event;Channel",100,0,100,NRCEChannels,0,NRCEChannels);
  hRCEDNoiseChannel                     = new TProfile("RCEDNoiseChannel","RCE DNoise_\"colz\"_none;Channel;DNoise",NRCEChannels,0,NRCEChannels);
  hTotalADCEvent                        = new TH1I("TotalADCEvent","Total RCE ADC_\"colz\"_logy;Total ADC;",100,0,1000000000);
  hTotalRCEHitsEvent                    = new TH1I("TotalRCEHitsEvent","Total RCE Hits in Event_\"colz\"_logy;Total RCE Hits;",100,0,10000000);
  hTotalRCEHitsChannel                  = new TH1I("TotalRCEHitsChannel","Total RCE Hits by Channel_\"colz\"_none;Channel;Total RCE Hits;",NRCEChannels,0,NRCEChannels);
  hNumMicroslicesInMillislice           = new TH1I("NumMicroslicesInMillislice","Number of Microslices in Millislice_\"colz\"_none;Millislice;Number of Microslices;",16,100,116);
  hNumNanoslicesInMicroslice            = new TH1I("NumNanoslicesInMicroslice","Number of Nanoslices in Microslice_\"colz\"_logy;Number of Nanoslices;",1000,0,1100);
  hNumNanoslicesInMillislice            = new TH1I("NumNanoslicesInMillislice","Number of Nanoslices in Millislice_\"colz\"_logy;Number of Nanoslices;",1000,0,11000);
  hTimesADCGoesOverThreshold            = new TH1I("TimesADCGoesOverThreshold","Times RCE ADC Over Threshold_\"colz\"_none;Times ADC Goes Over Threshold;",100,0,1000);
  hAsymmetry                            = new TProfile("Asymmetry","Asymmetry of Bipolar Pulse_\"colz\"_none;Channel;Asymmetry",NRCEChannels,0,NRCEChannels);
  hRCEBitCheckAnd                       = new TH2I("RCEBitCheckAnd","RCE ADC Bits Always On_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  hRCEBitCheckOr                        = new TH2I("RCEBitCheckOr","RCE ADC Bits Always Off_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  hLastSixBitsCheckOn                   = new TProfile("LastSixBitsCheckOn","Last Six RCE ADC Bits On_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  hLastSixBitsCheckOff                  = new TProfile("LastSixBitsCheckOff","Last Six RCE ADC Bits Off_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  hAvADCAllMillislice                   = new TH1D("AvADCAllMillislice","Av ADC for all Millislices_\"colz\"_none;Event;Av ADC",10000,0,10000);
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannel[channel]                = new TProfile("ADCChannel"+TString(std::to_string(channel)),"RCE ADC v Tick for Channel "+TString(std::to_string(channel))+";Tick;ADC;",5000,0,5000);
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice) {
    hAvADCMillislice[millislice]        = new TH1D("AvADCMillislice"+TString(std::to_string(millislice)),"Av ADC for Millislice "+TString(std::to_string(millislice))+";Event;Av ADC;",10000,0,10000);
    hAvADCMillisliceChannel[millislice] = new TH1D("AvADCMillisliceChannel"+TString(std::to_string(millislice)),"Av ADC v Channel for Millislice "+TString(std::to_string(millislice))+";Channel;Av ADC;",128,0,128);
  }
  for (std::vector<int>::const_iterator debugchannel = DebugChannels.begin(); debugchannel != DebugChannels.end(); ++debugchannel)
    hDebugChannelHists[(*debugchannel)] = new TH1D("Channel"+TString(std::to_string(*debugchannel))+"SingleEvent","Channel "+TString(std::to_string(*debugchannel))+" for Single Event",5000,0,5000);

  // SSP hists
  hWaveformMeanChannel            = new TProfile("WaveformMeanChannel","SSP ADC Mean_\"histl\"_none;Channel;SSP ADC Mean",NSSPChannels,0,NSSPChannels);
  hWaveformRMSChannel             = new TProfile("WaveformRMSChannel","SSP ADC RMS_\"histl\"_none;Channel;SSP ADC RMS",NSSPChannels,0,NSSPChannels);
  hAvWaveformChannelEvent         = new TH2D("AvWaveformChannelEvent","Average SSP ADC Value_\"colz\"_none;Event;Channel",100,0,100,NSSPChannels,0,NSSPChannels);
  hSSPDNoiseChannel               = new TProfile("SSPDNoiseChannel","SSP DNoise_\"colz\"_none;Channel;DNoise",NSSPChannels,0,NSSPChannels);
  hTotalWaveformEvent             = new TH1I("TotalWaveformEvent","Total SSP ADC_\"colz\"_logy;Total Waveform;",100,0,1000000000);
  hTotalSSPHitsEvent              = new TH1I("TotalSSPHitsEvent","Total SSP Hits in Event_\"colz\"_logy;Total SSP Hits;",100,0,10000000);
  hTotalSSPHitsChannel            = new TH1I("TotalSSPHitsChannel","Total SSP Hits by Channel_\"colz\"_none;Channel;Total SSP Hits;",NSSPChannels,0,NSSPChannels);
  hTimesWaveformGoesOverThreshold = new TH1I("TimesWaveformGoesOverThreshold","Times SSP ADC Over Threshold_\"colz\"_none;Time Waveform Goes Over Threshold;",100,0,1000);
  hSSPBitCheckAnd                 = new TH2I("SSPBitCheckAnd","SSP ADC Bits Always On_\"colz\"_none;Channel;Bit",NSSPChannels,0,NSSPChannels,16,0,16);
  hSSPBitCheckOr                  = new TH2I("SSPBitCheckOr","SSP ADC Bits Always Off_\"colz\"_none;Channel;Bit",NSSPChannels,0,NSSPChannels,16,0,16);
  for (unsigned int channel = 0; channel < NSSPChannels; ++channel)
    hWaveformChannel[channel]     = new TProfile("WaveformChannel"+TString(std::to_string(channel)),"SSP ADC v Tick for Channel "+TString(std::to_string(channel))+";Tick;Waveform;",5000,0,5000);

  // General
  hNumSubDetectorsPresent = new TH1I("NumSubDetectorsPresent","Number of Subdetectors_\"colz\"_logy;Number of Subdetectors;",25,0,25);
  hSizeOfFiles            = new TH1I("SizeOfFiles","Data File Sizes_\"colz\"_none;Run&Subrun;Size (bytes);",20,0,20);
  hSizeOfFilesPerEvent    = new TH1D("SizeOfFilesPerEvent","Size of Event in Data Files_\"colz\"_none;Run&Subrun;Size (bytes/event);",20,0,20);

  // PTB hists
  hPTBTSUCounterHitRateWU = new TProfile("PTBTSUCounterRateWU","PTB TSU counter hit Rate (per millislice) (West Up)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBTSUCounterHitRateWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateWU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeWU = new TProfile("PTBTSUCounterActivationTimeWU","PTB counter average activation time (West Up)_\"\"_none;Counter number; Time",10,1,11);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateEL = new TProfile("PTBTSUCounterRateEL","PTB TSU counter hit Rate (per millislice) (East Low)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBTSUCounterHitRateEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateEL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeEL = new TProfile("PTBTSUCounterActivationTimeEL","PTB counter average activation time (East Low)_\"\"_none;Counter number; Time",10,1,11);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateExtra = new TProfile("PTBTSUCounterRateExtra","PTB TSU counter hit Rate (per millislice) (Empty counter bits - SHOULD BE EMPTY)_\"\"_none;Counter number; No. hits per millislice",4,1,5);
  hPTBTSUCounterHitRateExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterHitRateExtra->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeExtra = new TProfile("PTBTSUCounterActivationTimeExtra","PTB counter average activation time (Empty counter bits - SHOULD BE EMPTY)_\"\"_none;Counter number; Time",4,1,5);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateNU = new TProfile("PTBTSUCounterRateNU","PTB TSU counter hit Rate (per millislice) (North Up)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeNU = new TProfile("PTBTSUCounterActivationTimeNU","PTB counter average activation time (North Up)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateSL = new TProfile("PTBTSUCounterRateSL","PTB TSU counter hit Rate (per millislice) (South Low)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeSL = new TProfile("PTBTSUCounterActivationTimeSL","PTB counter average activation time (South Low)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateNL = new TProfile("PTBTSUCounterRateNL","PTB TSU counter hit Rate (per millislice) (North Low)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeNL = new TProfile("PTBTSUCounterActivationTimeNL","PTB counter average activation time (North Low)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateSU = new TProfile("PTBTSUCounterRateSU","PTB TSU counter hit Rate (per millislice) (South Up)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeSU = new TProfile("PTBTSUCounterActivationTimeSU","PTB counter average activation time (South Up)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateRM = new TProfile("PTBBSUCounterRateRM","PTB BSU counter hit Rate (per millislice) (RM)_\"\"_none;Counter number; No. hits per millislice",16,1,17);
  hPTBBSUCounterHitRateRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterHitRateRM->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeRM = new TProfile("PTBBSUCounterActivationTimeRM","PTB counter average activation time (RM)_\"\"_none;Counter number; Time",16,1,17);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateCU = new TProfile("PTBBSUCounterRateCU","PTB BSU counter hit Rate (per millislice) (CU)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBBSUCounterHitRateCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateCU->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeCU = new TProfile("PTBBSUCounterActivationTimeCU","PTB counter average activation time (CU)_\"\"_none;Counter number; Time",10,1,11);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateCL = new TProfile("PTBBSUCounterRateCL","PTB BSU counter hit Rate (per millislice) (CL)_\"\"_none;Counter number; No. hits per millislice",13,1,14);
  hPTBBSUCounterHitRateCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterHitRateCL->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeCL = new TProfile("PTBBSUCounterActivationTimeCL","PTB counter average activation time (CL)_\"\"_none;Counter number; Time",13,1,14);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateRL = new TProfile("PTBBSUCounterRateRL","PTB BSU counter hit Rate (per millislice) (RL)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBBSUCounterHitRateRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateRL->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeRL = new TProfile("PTBBSUCounterActivationTimeRL","PTB counter average activation time (RL)_\"\"_none;Counter number; Time",10,1,11);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->CenterLabels();

  hPTBTriggerRates = new TProfile("PTBTriggerRates","Muon trigger rates_\"\"_none;Muon trigger name; No. hits per millislice",4,1,5);
  hPTBTriggerRates->GetXaxis()->SetBinLabel(1,"BSU RM-CM");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(2,"TSU NU-SL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(3,"TSU SU-NL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(4,"TSU EL-WU");

  // Add all histograms to the array for saving
  AddHists();

}

void OnlineMonitoring::MonitoringData::EndMonitoring() {

  /// Clear up after writing out the monitoring data

  // Free the memory for the histograms
  fHistArray.Delete();

  // Free up all used memory
  // for (unsigned int interestingchannel = 0; interestingchannel < DebugChannels.size(); ++interestingchannel)
  //   hDebugChannelHists.at(DebugChannels.at(interestingchannel))->Delete();
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice)
    hAvADCMillislice.at(millislice)->Delete();
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannel.at(channel)->Delete();
  for (unsigned int channel = 0; channel < NSSPChannels; ++channel)
    hWaveformChannel.at(channel)->Delete();

  fDataFile->Close();
  delete fDataTree;
  delete fDataFile;
  delete fCanvas;

}

void OnlineMonitoring::MonitoringData::FillTree(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter) {
  fRCEADC = rceformatter.ADCVector();
  fSSPADC = sspformatter.ADCVector();
  fDataTree->Fill();
}

void OnlineMonitoring::MonitoringData::GeneralMonitoring() {

  /// Fills the general monitoring histograms (i.e. monitoring not specific to a particular hardware component)

  hNumSubDetectorsPresent->Fill(fNumSubDetectorsPresent);

  // Size of files
  if (!checkedFileSizes) {
    checkedFileSizes = true;
    std::multimap<Long_t,std::pair<std::vector<TString>,Long_t>,std::greater<Long_t> > fileMap;

    TSystemDirectory dataDir("dataDir",DataDirName);
    const TList *files = dataDir.GetListOfFiles();
    if (files) {
      TSystemFile *file;
      TString fileName;
      Long_t id,modified,size,flags;
      TIter next(files);
      while ( (file = (TSystemFile*)next()) ) {
	fileName = file->GetName();
    	if ( !file->IsDirectory() && fileName.EndsWith(".root") && !fileName.BeginsWith("lbne_r-") ) {
	  const TString path = DataDirName+TString(file->GetName());
    	  gSystem->GetPathInfo(path.Data(),&id,&size,&flags,&modified);
	  TObjArray *splitName = path.Tokenize(PathDelimiter);
	  if (splitName->GetEntriesFast() == 1)
	    continue;
	  std::vector<TString> name = {path,TString(splitName->At(1)->GetName()),TString(splitName->At(2)->GetName())};
	  fileMap.insert(std::pair<Long_t,std::pair<std::vector<TString>,Long_t> >(modified,std::pair<std::vector<TString>,Long_t>(name,size)));
    	}
      }
    }
    int i = 0;
    for(std::multimap<Long_t,std::pair<std::vector<TString>,Long_t> >::iterator mapIt = fileMap.begin(); mapIt != fileMap.end(); ++mapIt) {
      TString name = mapIt->second.first.at(1)+mapIt->second.first.at(2);
      if (_verbose)
	std::cout << "File: modified... " << mapIt->first << ", run " << name << " and size: " << mapIt->second.second << std::endl;
      ++i;
      std::stringstream cmd; cmd << "TFile::Open(\"" << mapIt->second.first.at(0) << "\"); Events->GetEntriesFast()";
      Int_t entries = gROOT->ProcessLine(cmd.str().c_str());
      hSizeOfFiles->GetXaxis()->SetBinLabel(i,name);
      hSizeOfFilesPerEvent->GetXaxis()->SetBinLabel(i,name);
      hSizeOfFiles->Fill(name,mapIt->second.second);
      hSizeOfFilesPerEvent->Fill(name,(double)((double)mapIt->second.second/(double)entries));
      if (i == 20) break;
    }
  }
}

void OnlineMonitoring::MonitoringData::RCEMonitoring(RCEFormatter const& rceformatter) {

  /// Fills all histograms pertaining to RCE hardware monitoring

  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();

  for (unsigned int channel = 0; channel < ADCs.size(); channel++) {

    if (!ADCs.at(channel).size())
      continue;

    // Variables for channel
    bool peak = false;
    int tTotalRCEHitsChannel = 0;
    bool tBitCheckAnd = 0xFFFF, tBitCheckOr = 0;
    double tAsymmetry = 0;
    double tADCsum = 0, tADCdiff = 0;

    // Find the mean and RMS of ADCs for this channel
    double mean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
    double rms  = TMath::RMS (ADCs.at(channel).begin(),ADCs.at(channel).end());

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); tick++) {

      int ADC = ADCs.at(channel).at(tick);

      // Fill hists for tick
      if (!_interestingchannelsfilled && std::find(DebugChannels.begin(), DebugChannels.end(), channel) != DebugChannels.end())
      	hDebugChannelHists.at(channel)->Fill(tick,ADC);
      hADCChannel.at(channel)->Fill(tick,ADC);
      if (channel && !ADCs.at(channel-1).empty()) hRCEDNoiseChannel->Fill(channel,ADC-ADCs.at(channel-1).at(tick));

      // Increase variables
      fTotalADC += ADC;
      if (ADC > fThreshold) {
	++tTotalRCEHitsChannel;
	++fTotalRCEHitsEvent;
      }

      // Times over threshold
      if ( (ADC > fThreshold) && !peak ) {
	++fTimesADCGoesOverThreshold;
	peak = true;
      }
      if ( tick && (ADC < ADCs.at(channel).at(tick-1)) && peak ) peak = false;

      // Bit check
      tBitCheckAnd &= ADC;
      tBitCheckOr  |= ADC;

      // Check last 6 bits
      int mask1 = 0xFFC0, mask2 = 0x003F;
      hLastSixBitsCheckOff->Fill(channel,((ADC & mask1) == ADC));
      hLastSixBitsCheckOn ->Fill(channel,((ADC & mask2) == ADC));

    }

    // Fill hists for channel
    hADCMeanChannel     ->Fill(channel,mean);
    hADCRMSChannel      ->Fill(channel,rms);
    hAvADCChannelEvent  ->Fill(fEventNumber,channel,mean);
    hTotalRCEHitsChannel->Fill(channel+1,tTotalRCEHitsChannel);
    int tbit = 1;
    for (int bitIt = 0; bitIt < 16; ++bitIt) {
      hRCEBitCheckAnd->Fill(channel,bitIt,(tBitCheckAnd & tbit));
      hRCEBitCheckOr ->Fill(channel,bitIt,(tBitCheckOr & tbit));
      tbit <<= 1;
    }

    // Loop over blocks to look at the asymmetry
    for (int block = 0; block < rceformatter.NumBlocks().at(channel); block++) {
      // Loop over the ticks within the block
      for (int tick = rceformatter.BlockBegin().at(channel).at(block); tick < rceformatter.BlockBegin().at(channel).at(block)+rceformatter.BlockSize().at(channel).at(block); tick++) {
	if (fIsInduction) {
	  tADCdiff += ADCs.at(channel).at(tick);
	  tADCsum += abs(ADCs.at(channel).at(tick));
	}
      } // End of tick loop
    } // End of block loop

    if (fIsInduction && tADCsum) tAsymmetry = (double)tADCdiff / (double)tADCsum;
    hAsymmetry->Fill(channel+1,tAsymmetry);

  }

  // Fill hists for event
  hTotalADCEvent            ->Fill(fTotalADC);
  hTotalRCEHitsEvent        ->Fill(fTotalRCEHitsEvent);
  hTimesADCGoesOverThreshold->Fill(fTimesADCGoesOverThreshold);

  // Find average ADCs for each millislice
  for (unsigned int millislice = 0; millislice < ADCs.size()/128; ++millislice) {
    std::vector<int> millisliceADCs;
    for (unsigned int channel = millislice*128; channel < (millislice*128)+127; ++channel)
      if (ADCs.at(channel).size()) {
	double millisliceMean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
	millisliceADCs.push_back(millisliceMean);
	hAvADCMillisliceChannel.at(millislice)->Fill(channel%128, millisliceMean);
      }
    if (!millisliceADCs.size()) continue;
    double mean = TMath::Mean(millisliceADCs.begin(),millisliceADCs.end());
    hAvADCMillislice.at(millislice)->Fill(fEventNumber, mean);
    hAvADCAllMillislice            ->Fill(fEventNumber, mean);

    _interestingchannelsfilled = true;
  }

}

void OnlineMonitoring::MonitoringData::SSPMonitoring(SSPFormatter const& sspformatter) {

  /// Fills all histograms pertaining to SSP hardware monitoring

  const std::vector<std::vector<int> > ADCs = sspformatter.ADCVector();

  for (unsigned int channel = 0; channel < ADCs.size(); channel++) {

    if (!ADCs.at(channel).size())
      continue;

    // Variables for channel
    bool peak = false;
    bool tBitCheckAnd = 0xFFFF, tBitCheckOr = 0;
    int tTotalSSPHitsChannel = 0;

    // Find the mean and RMS Waveform for this channel
    double mean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
    double rms  = TMath::RMS (ADCs.at(channel).begin(),ADCs.at(channel).end());

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); tick++) {

      int ADC = ADCs.at(channel).at(tick);

      // Fill hists for tick
      hWaveformChannel.at(channel)->Fill(tick,ADC);
      if (channel and !ADCs.at(channel-1).empty()) hSSPDNoiseChannel->Fill(channel,ADC-ADCs.at(channel-1).at(tick));

      // Increase variables
      fTotalWaveform += ADC;
      ++fTotalSSPHitsEvent;
      ++tTotalSSPHitsChannel;

      // Times over threshold
      if ( (ADC > fThreshold) && !peak ) {
	++fTimesWaveformGoesOverThreshold;
	peak = true;
      }
      if ( tick && (ADC < ADCs.at(channel).at(tick-1)) && peak ) peak = false;

      // Bit check
      tBitCheckAnd &= ADC;
      tBitCheckOr  |= ADC;

    }

    // Fill hists for channel
    hWaveformMeanChannel->Fill(channel,mean);
    hWaveformRMSChannel->Fill(channel,rms);
    hAvWaveformChannelEvent->Fill(fEventNumber,channel,mean);
    hTotalSSPHitsChannel->Fill(channel+1,tTotalSSPHitsChannel);
    int tbit = 1;
    for (int bitIt = 0; bitIt < 16; ++bitIt) {
      hSSPBitCheckAnd->Fill(channel,bitIt,(tBitCheckAnd & tbit));
      hSSPBitCheckOr ->Fill(channel,bitIt,(tBitCheckOr & tbit));
      tbit <<= 1;
    }

  }

  // Fill hists for event
  hTotalWaveformEvent->Fill(fTotalADC);
  hTotalSSPHitsEvent->Fill(fTotalSSPHitsEvent);
  hTimesWaveformGoesOverThreshold->Fill(fTimesADCGoesOverThreshold);

}

void OnlineMonitoring::MonitoringData::PTBMonitoring(PTBFormatter const& ptb_formatter) {

  /// Produces PTB monitoring histograms

  if (ptb_formatter.NumTriggers() == 0)
    return;

  double activation_time = 0;
  int hit_rate = 0;

  for (int i = 0; i < 97; i++) {

    ptb_formatter.AnalyzeCounter(i,activation_time,hit_rate);

    //Now we need to fill the relevant histograms :(
    if (i>=0 && i<=9){
      //Dealing with the WU TSU counters
      hPTBTSUCounterHitRateWU->Fill(i+1,hit_rate);
      hPTBTSUCounterActivationTimeWU->Fill(i+1,activation_time);
    }
    else if (i>=10 && i<=19){
      //Dealing with the EL TSU counters
      hPTBTSUCounterHitRateEL->Fill(i+1-10,hit_rate);
      hPTBTSUCounterActivationTimeEL->Fill(i+1-10,activation_time);
    }
    else if (i>=20 && i<=23){
      //Dealing with the EL TSU counters
      hPTBTSUCounterHitRateExtra->Fill(i+1-20,hit_rate);
      hPTBTSUCounterActivationTimeExtra->Fill(i+1-20,activation_time);
    }
    else if (i>=24 && i<=29){
      //Dealing with the NU TSU counters
      hPTBTSUCounterHitRateNU->Fill(i+1-24,hit_rate);
      hPTBTSUCounterActivationTimeNU->Fill(i+1-24,activation_time);
    }
    else if (i>=30 && i<=35){
      //Dealing with the SL TSU counters
      hPTBTSUCounterHitRateSL->Fill(i+1-30,hit_rate);
      hPTBTSUCounterActivationTimeSL->Fill(i+1-30,activation_time);
    }
    else if (i>=36 && i<=41){
      //Dealing with the NL TSU counters
      hPTBTSUCounterHitRateNL->Fill(i+1-36,hit_rate);
      hPTBTSUCounterActivationTimeNL->Fill(i+1-36,activation_time);
    }
    else if (i>=42 && i<=47){
      //Dealing with the SU TSU counters
      hPTBTSUCounterHitRateSU->Fill(i+1-42,hit_rate);
      hPTBTSUCounterActivationTimeSU->Fill(i+1-42,activation_time);
    }
    //In the word map, the BSU counters are 0 indexed, but start at 48.  Let the compiler do that maths
    else if (i>=0+48 && i<=15+48){
      //Dealing with the RM BSU counters
      hPTBBSUCounterHitRateRM->Fill(i+1-(0+48),hit_rate);
      hPTBBSUCounterActivationTimeRM->Fill(i+1-(0+48),activation_time);
    }
    else if (i>=16+48 && i<=25+48){
      //Dealing with the CU BSU counters
      hPTBBSUCounterHitRateCU->Fill(i+1-(16+48),hit_rate);
      hPTBBSUCounterActivationTimeCU->Fill(i+1-(16+48),activation_time);
    }
    else if (i>=26+48 && i<=38+48){
      //Dealing with the CL BSU counters
      hPTBBSUCounterHitRateCL->Fill(i+1-(26+48),hit_rate);
      hPTBBSUCounterActivationTimeCL->Fill(i+1-(26+48),activation_time);
    }
    else if (i>=39+48 && i<=48+48){
      //Dealing with the RL BSU counters
      hPTBBSUCounterHitRateRL->Fill(i+1-(39+48),hit_rate);
      hPTBBSUCounterActivationTimeRL->Fill(i+1-(39+48),activation_time);
    }

    //Now do the triggers
    int trigger_rate = 0;
    ptb_formatter.AnalyzeMuonTrigger(1,trigger_rate);
    hPTBTriggerRates->Fill(1,trigger_rate);

    ptb_formatter.AnalyzeMuonTrigger(2,trigger_rate);
    hPTBTriggerRates->Fill(2,trigger_rate);

    ptb_formatter.AnalyzeMuonTrigger(4,trigger_rate);
    hPTBTriggerRates->Fill(3,trigger_rate);

    ptb_formatter.AnalyzeMuonTrigger(8,trigger_rate);
    hPTBTriggerRates->Fill(4,trigger_rate);

  }

  return;

}

void OnlineMonitoring::MonitoringData::WriteMonitoringData(int run, int subrun) {

  /// Writes all the monitoring data currently saved in the data objects

  // Make the html for the web pages
  ofstream imageHTML((HistSaveDirectory+TString("index.html").Data()));
  imageHTML << "<head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style/style.css\"><title>35t: Run " << run << ", Subrun " << subrun <<"</title></head>" << std::endl << "<body><div class=\"bannertop\"></div>";
  imageHTML << "<h1 align=center>Monitoring for Run " << run << ", Subrun " << subrun << "</h1>" << std::endl;

  fDataFile->cd();

  // Write the tree
  if (fMakeTree) fDataTree->Write();

  // Save all the histograms as images and write to file
  for (int histIt = 0; histIt < fHistArray.GetEntriesFast(); ++histIt) {
    fCanvas->cd();
    TH1 *_h = (TH1*)fHistArray.At(histIt);
    TObjArray *histTitle = TString(_h->GetTitle()).Tokenize(PathDelimiter);
    _h->Draw((char*)histTitle->At(1)->GetName());
    TPaveText *canvTitle = new TPaveText(0.05,0.92,0.6,0.98,"brNDC");
    canvTitle->AddText((std::string(histTitle->At(0)->GetName())+": Run "+std::to_string(run)+", SubRun "+std::to_string(subrun)).c_str());
    canvTitle->Draw();
    if (strstr(histTitle->At(2)->GetName(),"logy")) fCanvas->SetLogy(1);
    else fCanvas->SetLogy(0);
    fCanvas->Modified();
    fCanvas->Update();
    fCanvas->SaveAs(HistSaveDirectory+TString(_h->GetName())+ImageType);
    fDataFile->cd();
    _h->Write();
    imageHTML << "<figure><a href=\"" << (TString(_h->GetName())+ImageType).Data() << "\"><img src=\"" << (TString(_h->GetName())+ImageType).Data() << "\" width=\"650\"></a><figcaption>" << fFigureCaptions.at(_h->GetName()) << "</figcaption></figure>" << std::endl;
  }

  TCanvas *mslicecanv = new TCanvas("mslicecanv","",800,600);
  TLegend *l = new TLegend(0.7,0.7,0.9,0.9,"Millislice","brNDC");
  int colour = 1;
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice, ++colour) {
    hAvADCMillisliceChannel.at(millislice)->SetLineColor(colour);
    if (colour == 1) hAvADCMillisliceChannel.at(millislice)->Draw();
    else hAvADCMillisliceChannel.at(millislice)->Draw("same");
    l->AddEntry(hAvADCMillisliceChannel.at(millislice),"Millislice "+TString(std::to_string(millislice)),"l");
    fDataFile->cd();
    hAvADCMillisliceChannel.at(millislice)->Write();
  }
  mslicecanv->cd();
  l->Draw("same");
  mslicecanv->SaveAs(HistSaveDirectory+TString("AvADCMillisliceChannel")+ImageType);
  delete mslicecanv;
  imageHTML << "<figure><a href=\"AvADCMillisliceChannel.png\"><img src=\"AvADCMillisliceChannel.png\" width=\"650\"></a><figcaption>" << "Average ADC count across the channels read out by each millislice present in the data." << "</figcaption></figure>" << std::endl;

  imageHTML << "<div class=\"bannerbottom\"></div></body>" << std::endl;
  imageHTML.close();

  // Write other histograms
  fDataFile->cd();
  for (unsigned int interestingchannel = 0; interestingchannel < DebugChannels.size(); ++interestingchannel)
    hDebugChannelHists.at(DebugChannels.at(interestingchannel))->Write();
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice)
    hAvADCMillislice.at(millislice)->Write();
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannel.at(channel)->Write();
  for (unsigned int channel = 0; channel < NSSPChannels; ++channel)
    hWaveformChannel.at(channel)->Write();

  // Add run file
  ofstream tmp((HistSaveDirectory+TString("run").Data()));
  tmp << run << " " << subrun;
  tmp.flush();
  tmp.close();
  system(("chmod -R a=rwx "+std::string(HistSaveDirectory)).c_str());  

  mf::LogInfo("Monitoring") << "Monitoring for run " << run << ", subRun " << subrun << " is viewable at http://lbne-dqm.fnal.gov/OnlineMonitoring/Run" << run << "Subrun" << subrun;

}

void OnlineMonitoring::MonitoringData::StartEvent(int eventNumber, bool maketree) {
  fEventNumber = eventNumber;
  fMakeTree = maketree;
  this->Reset();
}

void OnlineMonitoring::MonitoringData::Reset() {

  fRCEADC.clear();
  fSSPADC.clear();

  fTotalADC = 0;
  fTotalWaveform = 0;

  fTotalRCEHitsEvent = 0;
  fTotalSSPHitsEvent = 0;

  fTimesADCGoesOverThreshold = 0;
  fTimesWaveformGoesOverThreshold = 0;

  fNumSubDetectorsPresent = 0;

}

void OnlineMonitoring::MonitoringData::AddHists() {

  /// Adds all the histograms to an array for easier handling

  // The order the histograms are added will be the order they're displayed on the web!
  fHistArray.Add(hADCMeanChannel); fHistArray.Add(hADCRMSChannel);
  fHistArray.Add(hAvADCAllMillislice); fHistArray.Add(hRCEDNoiseChannel);
  fHistArray.Add(hAvADCChannelEvent); fHistArray.Add(hTotalRCEHitsChannel);
  fHistArray.Add(hTotalADCEvent); fHistArray.Add(hTotalRCEHitsEvent);
  fHistArray.Add(hAsymmetry); fHistArray.Add(hTimesADCGoesOverThreshold);
  fHistArray.Add(hRCEBitCheckAnd); fHistArray.Add(hRCEBitCheckOr);
  fHistArray.Add(hLastSixBitsCheckOn); fHistArray.Add(hLastSixBitsCheckOff);
  fHistArray.Add(hNumMicroslicesInMillislice); 

  fHistArray.Add(hAvWaveformChannelEvent);
  fHistArray.Add(hWaveformMeanChannel); fHistArray.Add(hWaveformRMSChannel);
  fHistArray.Add(hSSPDNoiseChannel); fHistArray.Add(hTotalSSPHitsChannel);
  fHistArray.Add(hTotalWaveformEvent); fHistArray.Add(hTotalSSPHitsEvent);
  fHistArray.Add(hSSPBitCheckAnd); fHistArray.Add(hSSPBitCheckOr);
  fHistArray.Add(hTimesWaveformGoesOverThreshold);

  fHistArray.Add(hSizeOfFiles); fHistArray.Add(hSizeOfFilesPerEvent);
  fHistArray.Add(hNumSubDetectorsPresent);

  //Penn board hists
  fHistArray.Add(hPTBTSUCounterActivationTimeWU);
  fHistArray.Add(hPTBTSUCounterHitRateWU);
  fHistArray.Add(hPTBTSUCounterActivationTimeEL);
  fHistArray.Add(hPTBTSUCounterHitRateEL);
  fHistArray.Add(hPTBTSUCounterActivationTimeExtra);
  fHistArray.Add(hPTBTSUCounterHitRateExtra);
  fHistArray.Add(hPTBTSUCounterActivationTimeNU);
  fHistArray.Add(hPTBTSUCounterHitRateNU);
  fHistArray.Add(hPTBTSUCounterActivationTimeSL);
  fHistArray.Add(hPTBTSUCounterHitRateSL);
  fHistArray.Add(hPTBTSUCounterActivationTimeNL);
  fHistArray.Add(hPTBTSUCounterHitRateNL);
  fHistArray.Add(hPTBTSUCounterActivationTimeSU);
  fHistArray.Add(hPTBTSUCounterHitRateSU);
  fHistArray.Add(hPTBBSUCounterActivationTimeRM);
  fHistArray.Add(hPTBBSUCounterHitRateRM);
  fHistArray.Add(hPTBBSUCounterActivationTimeCU);
  fHistArray.Add(hPTBBSUCounterHitRateCU);
  fHistArray.Add(hPTBBSUCounterActivationTimeCL);
  fHistArray.Add(hPTBBSUCounterHitRateCL);
  fHistArray.Add(hPTBBSUCounterActivationTimeRL);
  fHistArray.Add(hPTBBSUCounterHitRateRL);
  fHistArray.Add(hPTBTriggerRates);

  fFigureCaptions["ADCMeanChannel"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["ADCRMSChannel"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["AvADCAllMillislice"] = "Average RCE ADC across an entire millislice for the first 10000 events (one entry per millislice in each event)";
  fFigureCaptions["RCEDNoiseChannel"] = "DNoise (difference in ADC between neighbouring channels for the same tick) for the RCE ADCs (profiled over all events read)";
  fFigureCaptions["AvADCChannelEvent"] = "Average RCE ADC across a channel for an event, shown for the first 100 events";
  fFigureCaptions["TotalRCEHitsChannel"] = "Total number of RCE hits in each channel across all events";
  fFigureCaptions["TotalADCEvent"] = "Total RCE ADC recorded for an event across all channels (one entry per event)";
  fFigureCaptions["TotalRCEHitsEvent"] = "Total number of hits recorded on the RCEs per event (one entry per event)";
  fFigureCaptions["Asymmetry"] = "Asymmetry of the bipolar pulse measured on the induction planes, by channel (profiled across all events). Zero means completely symmetric pulse";
  fFigureCaptions["TimesADCGoesOverThreshold"] = "Number of times an RCE hit goes over a set ADC threshold";
  fFigureCaptions["RCEBitCheckAnd"] = "Check for stuck RCE bits: bits which are always on";
  fFigureCaptions["RCEBitCheckOr"] = "Check for stuck RCE bits: bits which are always off";
  fFigureCaptions["LastSixBitsCheckOn"] = "Fraction of all RCE ADC values with the last six bits stuck on (profiled; one entry per ADC)";
  fFigureCaptions["LastSixBitsCheckOff"] = "Fraction of all RCE ADC values with the last six bits stuck off (profiled; one entry per ADC)";
  fFigureCaptions["NumMicroslicesInMillislice"] = "Number of microslices in a millislice in this run";
  fFigureCaptions["AvWaveformChannelEvent"] = "Average SSP ADC across a channel for an event, shown for the first 100 events";
  fFigureCaptions["WaveformMeanChannel"] = "Mean ADC values for each channel read out by the SSPs (profiled over all events read)";
  fFigureCaptions["WaveformRMSChannel"] = "RMS of the ADC values for each channel read out by the SSPs (profiled over all events read)";
  fFigureCaptions["SSPDNoiseChannel"] = "DNoise (difference in ADC between neighbouring channels for the same tick) for the RCE ADCs (profiled over all events read)";
  fFigureCaptions["TotalSSPHitsChannel"] = "Total number of SSP hits in each channel across all events";
  fFigureCaptions["TotalWaveformEvent"] = "Total SSP ADC recorded for an event across all channels (one entry per event)";
  fFigureCaptions["TotalSSPHitsEvent"] = "Total number of hits recorded on the SSPs per event (one entry per event)";
  fFigureCaptions["SSPBitCheckAnd"] = "Check for stuck SSP bits: bits which are always on";
  fFigureCaptions["SSPBitCheckOr"] = "Check for stuck SSP bits: bits which are always off";
  fFigureCaptions["TimesWaveformGoesOverThreshold"] = "Number of times an SSP hit goes over a set ADC threshold";
  fFigureCaptions["SizeOfFiles"] = "Size of the data files made by the DAQ for the last 20 runs";
  fFigureCaptions["SizeOfFilesPerEvent"] = "Size of event in each of the last 20 data files made by the DAQ (size of file / number of events in file)";
  fFigureCaptions["NumSubDetectorsPresent"] = "Number of subdetectors present in each event in the data (one entry per event)";

  //Penn board captions
  fFigureCaptions["PTBTSUCounterActivationTimeWU"] = "Average activation time for the TSU West Up counters";
  fFigureCaptions["PTBTSUCounterRateWU"] = "Average hit rate in a millislice for the TSU West Up counters";
  fFigureCaptions["PTBTSUCounterActivationTimeEL"] = "Average activation time for the TSU East Low counters";
  fFigureCaptions["PTBTSUCounterRateEL"] = "Average hit rate in a millislice for the TSU East Low counters";
  fFigureCaptions["PTBTSUCounterActivationTimeExtra"] = "Average activation time for the TSU extra counter bits (SHOULD BE EMPTY)";
  fFigureCaptions["PTBTSUCounterRateExtra"] = "Average hit rate in a millislice for the TSU extra counter bits (SHOULD BE EMPTY)";
  fFigureCaptions["PTBTSUCounterActivationTimeNU"] = "Average activation time for the TSU North Up counters";
  fFigureCaptions["PTBTSUCounterRateNU"] = "Average hit rate in a millislice for the TSU North Up counters";
  fFigureCaptions["PTBTSUCounterActivationTimeSL"] = "Average activation time for the TSU South Low counters";
  fFigureCaptions["PTBTSUCounterRateSL"] = "Average hit rate in a millislice for the TSU South Low counters";
  fFigureCaptions["PTBTSUCounterActivationTimeNL"] = "Average activation time for the TSU North Low counters";
  fFigureCaptions["PTBTSUCounterRateNL"] = "Average hit rate in a millislice for the TSU North Low counters";
  fFigureCaptions["PTBTSUCounterActivationTimeSU"] = "Average activation time for the TSU North Low counters";
  fFigureCaptions["PTBTSUCounterRateSU"] = "Average hit rate in a millislice for the TSU North Low counters";
  fFigureCaptions["PTBBSUCounterActivationTimeRM"] = "Average activation time for the BSU RM counters";
  fFigureCaptions["PTBBSUCounterRateRM"] = "Average hit rate in a millislice for the BSU RM counters";
  fFigureCaptions["PTBBSUCounterActivationTimeCU"] = "Average activation time for the BSU CU counters";
  fFigureCaptions["PTBBSUCounterRateCU"] = "Average hit rate in a millislice for the BSU CU counters";
  fFigureCaptions["PTBBSUCounterActivationTimeCL"] = "Average activation time for the BSU CL counters";
  fFigureCaptions["PTBBSUCounterRateCL"] = "Average hit rate in a millislice for the BSU CL counters";
  fFigureCaptions["PTBBSUCounterActivationTimeRL"] = "Average activation time for the BSU RL counters";
  fFigureCaptions["PTBBSUCounterRateRL"] = "Average hit rate in a millislice for the BSU RL counters";
  fFigureCaptions["PTBTriggerRates"] = "Average hit rates per millislice of the muon trigger system";

}
