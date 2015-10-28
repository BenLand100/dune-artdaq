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
  //receivedData = false; _interestingchannelsfilled = false;
  filledRunData = false;

  // Get directory for this run
  std::ostringstream directory;
  directory << HistSavePath << "Run" << run << "Subrun" << subrun << "/";
  HistSaveDirectory = directory.str();

  // Make the directories to save the files
  std::ostringstream cmd;
  cmd << "touch " << directory.str() << "; rm -rf " << directory.str() << "; mkdir " << directory.str()
      << "; mkdir " << directory.str() << "General"
      << "; mkdir " << directory.str() << "RCE"
      << "; mkdir " << directory.str() << "SSP"
      << "; mkdir " << directory.str() << "PTB";
  system(cmd.str().c_str());

  // Outfile
  fDataFile = new TFile(HistSaveDirectory+TString("monitoringRun"+std::to_string(run)+"Subrun"+std::to_string(subrun)+".root"),"RECREATE");
  fDataFile->mkdir("General");
  fDataFile->mkdir("RCE");
  fDataFile->mkdir("SSP");
  fDataFile->mkdir("PTB");

  // Tree
  if (fMakeTree) {
    fDataTree = new TTree("RawData","Raw Data");
    fDataTree->Branch("RCE",&fRCEADC);
    fDataTree->Branch("SSP",&fSSPADC);
  }

  this->MakeHistograms();

}

void OnlineMonitoring::MonitoringData::EndMonitoring() {

  /// Clear up after writing out the monitoring data

  // Free the memory for the histograms
  fHistArray.Delete();

  // Free up all used memory
  // for (unsigned int interestingchannel = 0; interestingchannel < DebugChannels.size(); ++interestingchannel)
  //   hDebugChannelHists.at(DebugChannels.at(interestingchannel))->Delete();
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannelMap.at(channel)->Delete();

  fDataFile->Close();
  delete fDataTree;
  delete fDataFile;
  delete fCanvas;

}

void OnlineMonitoring::MonitoringData::FillTree(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter) {
  fRCEADC = rceformatter.ADCVector();
  // fSSPADC = sspformatter.ADCVector();
  std::cout << "Number of SSPs is " << sspformatter.NumSSPs << std::endl;
  fDataTree->Fill();
}

void OnlineMonitoring::MonitoringData::GeneralMonitoring(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter, PTBFormatter const& ptbformatter) {

  /// Fills the general monitoring histograms (i.e. monitoring not specific to a particular hardware component)

  // Subdetectors with data
  hNumSubDetectorsPresent->Fill(rceformatter.NumRCEs + sspformatter.NumSSPs + ptbformatter.PTBData);
  for (std::vector<std::string>::const_iterator compIt = DAQComponents.begin(); compIt != DAQComponents.end(); ++compIt)
    if ( (std::find(rceformatter.RCEsWithData.begin(), rceformatter.RCEsWithData.end(), *compIt) != rceformatter.RCEsWithData.end()) or
	 (std::find(sspformatter.SSPsWithData.begin(), sspformatter.SSPsWithData.end(), *compIt) != sspformatter.SSPsWithData.end()) or
	 (*compIt == "PTB" and ptbformatter.PTBData) ) {
      hSubDetectorsPresent->Fill((*compIt).c_str(),1);
      hSubDetectorsWithData->SetBinContent(std::distance(DAQComponents.begin(),compIt)+1,1);
    }

  // Fill data just once per run
  if (!filledRunData) {
    filledRunData = true;

    // Size of files
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
	  size /= 1e6;
	  TObjArray *splitName = path.Tokenize(PathDelimiter);
	  if (splitName->GetEntriesFast() == 1)
	    continue;
	  std::vector<TString> name = {path,TString(splitName->At(1)->GetName()),TString(splitName->At(2)->GetName())};
	  fileMap.insert(std::pair<Long_t,std::pair<std::vector<TString>,Long_t> >(modified,std::pair<std::vector<TString>,Long_t>(name,size)));
    	}
      }
    }
    int i = 1;
    for(std::multimap<Long_t,std::pair<std::vector<TString>,Long_t> >::iterator mapIt = fileMap.begin(); mapIt != fileMap.end(); ++mapIt, ++i) {
      TString name = mapIt->second.first.at(1)+mapIt->second.first.at(2);
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

  // Variables for event
  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();
  int totalADC = 0, totalRCEHitsEvent = 0, timesADCGoesOverThreshold = 0;

  for (unsigned int channel = 0; channel < ADCs.size(); channel++) {

    if (!ADCs.at(channel).size())
      continue;

    // Variables for channel
    bool peak = false;
    int totalRCEHitsChannel = 0;
    bool bitCheckAnd = 0xFFFF, bitCheckOr = 0;
    double asymmetry = 0;
    double ADCsum = 0, ADCdiff = 0;

    // Find the mean and RMS of ADCs for this channel
    double mean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
    double rms  = TMath::RMS (ADCs.at(channel).begin(),ADCs.at(channel).end());

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); tick++) {

      int ADC = ADCs.at(channel).at(tick);

      // Fill hists for tick
      hADCChannelMap.at(channel)->Fill(tick,ADC);
      if (channel && !ADCs.at(channel-1).empty() && tick < ADCs.at(channel-1).size())
	hRCEDNoiseChannel->Fill(channel,ADC-ADCs.at(channel-1).at(tick));
      hADCChannel->Fill(channel,ADC,1);

      // Debug
      if (!_interestingchannelsfilled && std::find(DebugChannels.begin(), DebugChannels.end(), channel) != DebugChannels.end())
      	hDebugChannelHists.at(channel)->Fill(tick,ADC);

      // Increase variables
      totalADC += ADC;
      if (ADC > fThreshold) {
	++totalRCEHitsChannel;
	++totalRCEHitsEvent;
      }

      // Times over threshold
      if ( (ADC > fThreshold) && !peak ) {
	++timesADCGoesOverThreshold;
	peak = true;
      }
      if ( tick && (ADC < ADCs.at(channel).at(tick-1)) && peak ) peak = false;

      // Bit check
      bitCheckAnd &= ADC;
      bitCheckOr  |= ADC;

      // Check last 6 bits
      int mask1 = 0xFFC0, mask2 = 0x003F;
      hLastSixBitsCheckOff->Fill(channel,((ADC & mask1) == ADC));
      hLastSixBitsCheckOn ->Fill(channel,((ADC & mask2) == ADC));

    }

    // Fill hists for channel
    hADCMeanChannelAPA1     ->Fill(channel,mean);
    hADCMeanChannelAPA2     ->Fill(channel,mean);
    hADCMeanChannelAPA3     ->Fill(channel,mean);
    hADCMeanChannelAPA4     ->Fill(channel,mean);
    hADCRMSChannelAPA1      ->Fill(channel,rms);
    hADCRMSChannelAPA2      ->Fill(channel,rms);
    hADCRMSChannelAPA3      ->Fill(channel,rms);
    hADCRMSChannelAPA4      ->Fill(channel,rms);
    hAvADCChannelEvent  ->Fill(fEventNumber,channel,mean);
    hTotalRCEHitsChannel->Fill(channel+1,totalRCEHitsChannel);
    int tbit = 1;
    for (int bitIt = 0; bitIt < 16; ++bitIt) {
      hRCEBitCheckAnd->Fill(channel,bitIt,(bitCheckAnd & tbit));
      hRCEBitCheckOr ->Fill(channel,bitIt,(bitCheckOr & tbit));
      tbit <<= 1;
    }

    // Loop over blocks to look at the asymmetry
    for (int block = 0; block < rceformatter.NumBlocks().at(channel); block++) {
      // Loop over the ticks within the block
      for (int tick = rceformatter.BlockBegin().at(channel).at(block); tick < rceformatter.BlockBegin().at(channel).at(block)+rceformatter.BlockSize().at(channel).at(block); tick++) {
	if (fIsInduction) {
	  ADCdiff += ADCs.at(channel).at(tick);
	  ADCsum += abs(ADCs.at(channel).at(tick));
	}
      } // End of tick loop
    } // End of block loop

    if (fIsInduction && ADCsum) asymmetry = (double)ADCdiff / (double)ADCsum;
    hAsymmetry->Fill(channel+1,asymmetry);

  }

  // Fill hists for event
  hTotalADCEvent            ->Fill(totalADC);
  hTotalRCEHitsEvent        ->Fill(totalRCEHitsEvent);
  hTimesADCGoesOverThreshold->Fill(timesADCGoesOverThreshold);

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

    _interestingchannelsfilled = true;
  }

}

void OnlineMonitoring::MonitoringData::SSPMonitoring(SSPFormatter const& sspformatter) {

  /// Fills all histograms pertaining to SSP hardware monitoring

  const std::map<int,std::vector<Trigger> > channelTriggers = sspformatter.ChannelTriggers();

  // Loop over channels
  for (std::map<int,std::vector<Trigger> >::const_iterator channelIt = channelTriggers.begin(); channelIt != channelTriggers.end(); ++channelIt) {

    const int channel = channelIt->first;
    const std::vector<Trigger> triggers = channelIt->second;

    // Loop over triggers
    for (std::vector<Trigger>::const_iterator triggerIt = triggers.begin(); triggerIt != triggers.end(); ++triggerIt) {

      // Fill trigger level hists
      hWaveformMean->Fill(channel,triggerIt->Mean);
      hWaveformRMS->Fill(channel,triggerIt->RMS);
      hWaveformPeakHeight->Fill(channel,triggerIt->PeakSum);
      hWaveformIntegral->Fill(channel,triggerIt->Integral);
      hWaveformIntegralNorm->Fill(channel,triggerIt->Integral/triggerIt->NTicks);
      hWaveformPedestal->Fill(channel,triggerIt->Pedestal);
      hWaveformNumTicks->Fill(channel,triggerIt->NTicks);

    }

    // Fill channel level hists
    hNumberOfTriggers->Fill(channel,triggers.size());

  }

  // Fill event level hists

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
  ofstream mainHTML((HistSaveDirectory+TString("index.html").Data()));
  std::map<std::string,std::unique_ptr<ofstream> > componentHTML;

  // Main page
  mainHTML << "<head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style/style.css\"><title>35t: Run " << run << ", Subrun " << subrun <<"</title></head>" << std::endl << "<body><div class=\"bannertop\"></div>";
  mainHTML << "<h1 align=center>Monitoring for Run " << run << ", Subrun " << subrun << "</h1>" << std::endl;

  // Component pages
  for (auto& component : {"General","RCE","SSP","PTB"}) {
    mainHTML << "</br><a href=\"" << component << "\">" << component << "</a>" << std::endl;
    componentHTML[component].reset(new ofstream((HistSaveDirectory+component+TString("/index.html")).Data()));
    *componentHTML[component] << "<head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../../style/style.css\"><title>35t: Run " << run << ", Subrun " << subrun <<"</title></head>" << std::endl << "<body><div class=\"bannertop\"></div>";
    *componentHTML[component] << "<h1 align=center>" << component << "</h1>" << std::endl;
  }

  fDataFile->cd();

  // Write the tree
  if (fMakeTree) fDataTree->Write();

  // Save all the histograms as images and write to file
  for (int histIt = 0; histIt < fHistArray.GetEntriesFast(); ++histIt) {
    fCanvas->cd();
    TH1 *_h = (TH1*)fHistArray.At(histIt);
    TObjArray *histTitle = TString(_h->GetTitle()).Tokenize(PathDelimiter);
    TObjArray *histName = TString(_h->GetName()).Tokenize(PathDelimiter);
    _h->Draw((char*)histTitle->At(1)->GetName());
    TPaveText *canvTitle = new TPaveText(0.05,0.92,0.6,0.98,"NDC");
    canvTitle->AddText((std::string(histTitle->At(0)->GetName())+": Run "+std::to_string(run)+", SubRun "+std::to_string(subrun)).c_str());
    canvTitle->SetBorderSize(1);
    canvTitle->Draw();
    if (strstr(histTitle->At(2)->GetName(),"logy")) fCanvas->SetLogy(1);
    else fCanvas->SetLogy(0);
    fCanvas->Modified();
    fCanvas->Update();
    fCanvas->SaveAs(HistSaveDirectory+TString(histName->At(0)->GetName())+TString("/")+TString(_h->GetName())+ImageType);
    fDataFile->cd();
    fDataFile->cd(histName->At(0)->GetName());
    _h->Write();
    *componentHTML[histName->At(0)->GetName()] << "<figure><a href=\"" << (TString(_h->GetName())+ImageType).Data() << "\"><img src=\"" << (TString(_h->GetName())+ImageType).Data() << "\" width=\"650\"></a><figcaption>" << fFigureCaptions.at(_h->GetName()) << "</figcaption></figure>" << std::endl;
  }

  mainHTML << "<div class=\"bannerbottom\"></div></body>" << std::endl;
  mainHTML.flush();
  mainHTML.close();
  for (auto& component : {"General","RCE","SSP","PTB"}) {
    *componentHTML.at(component) << "<div class=\"bannerbottom\"></div></body>" << std::endl;
    componentHTML.at(component)->flush();
    componentHTML.at(component)->close();
  }

  // Write other histograms
  fDataFile->cd();
  fDataFile->cd("RCE");
  for (unsigned int interestingchannel = 0; interestingchannel < DebugChannels.size(); ++interestingchannel)
    hDebugChannelHists.at(DebugChannels.at(interestingchannel))->Write();
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannelMap.at(channel)->Write();

  // Add run file
  ofstream tmp((HistSaveDirectory+TString("run").Data()));
  tmp << run << " " << subrun;
  tmp.flush();
  tmp.close();

  mf::LogInfo("Monitoring") << "Monitoring for run " << run << ", subRun " << subrun << " is viewable at http://lbne-dqm.fnal.gov/OnlineMonitoring/Run" << run << "Subrun" << subrun;

}

void OnlineMonitoring::MonitoringData::StartEvent(int eventNumber, bool maketree) {
  fEventNumber = eventNumber;
  fMakeTree = maketree;
  fRCEADC.clear();
  fSSPADC.clear();
}

void OnlineMonitoring::MonitoringData::MakeHistograms() {

  /// Makes all histograms and adds them to an array for easier handling

  // Define all histograms
  // Nomenclature: Name is used to save the histogram, Title has format : histogramTitle_histDrawOptions_canvasOptions;x-title;y-title;z-title

  // General
  hNumSubDetectorsPresent = new TH1I("General__NumberOfSubdetectors_Total__All","Number of Subdetectors_\"colz\"_logy;Number of Subdetectors;",24,0,24);
  hSubDetectorsWithData   = new TH1I("General__SubdetectorsWithData_Present__All","Subdetectors With Data_\"colz\"_none;Subdetectors With Data;",24,0,24);
  hSubDetectorsPresent    = new TH1I("General__SubdetectorsPresent_Total__All","Subdetectors With Data (per event)_\"colz\"_none;Subdetectors With Data;",24,0,24);
  hSizeOfFiles            = new TH1I("General__Last20Files_Size_RunSubrun_All","Data File Sizes_\"colz\"_none;Run&Subrun;Size (MB);",20,0,20);
  hSizeOfFilesPerEvent    = new TH1D("General__Last20Files_SizePerEvent_RunSubrun_All","Size of Event in Data Files_\"colz\"_none;Run&Subrun;Size (MB/event);",20,0,20);
  for (std::vector<std::string>::const_iterator compIt = DAQComponents.begin(); compIt != DAQComponents.end(); ++compIt) {
    hSubDetectorsWithData->GetXaxis()->SetBinLabel(std::distance(DAQComponents.begin(),compIt)+1, (*compIt).c_str());
    hSubDetectorsPresent->GetXaxis()->SetBinLabel(std::distance(DAQComponents.begin(),compIt)+1, (*compIt).c_str());
  }
  hSubDetectorsWithData->GetXaxis()->SetLabelSize(0.025);
  hSubDetectorsPresent->GetXaxis()->SetLabelSize(0.025);

  // RCE hists
  hADCMeanChannelAPA1                   = new TProfile("RCE_APA0_ADC_Mean_Channel_All","RCE ADC Mean APA0_\"histl\"_none;Channel;RCE ADC Mean",512,0,511);
  hADCMeanChannelAPA2                   = new TProfile("RCE_APA1_ADC_Mean_Channel_All","RCE ADC Mean APA1_\"histl\"_none;Channel;RCE ADC Mean",512,512,1023);
  hADCMeanChannelAPA3                   = new TProfile("RCE_APA2_ADC_Mean_Channel_All","RCE ADC Mean APA2_\"histl\"_none;Channel;RCE ADC Mean",512,1024,1535);
  hADCMeanChannelAPA4                   = new TProfile("RCE_APA3_ADC_Mean_Channel_All","RCE ADC Mean APA3_\"histl\"_none;Channel;RCE ADC Mean",512,1536,2047);
  hADCRMSChannelAPA1                    = new TProfile("RCE_APA0_ADC_RMS_Channel_All","RCE ADC RMS APA0_\"histl\"_none;Channel;RCE ADC RMS",512,0,511);
  hADCRMSChannelAPA2                    = new TProfile("RCE_APA1_ADC_RMS_Channel_All","RCE ADC RMS APA1_\"histl\"_none;Channel;RCE ADC RMS",512,512,1023);
  hADCRMSChannelAPA3                    = new TProfile("RCE_APA2_ADC_RMS_Channel_All","RCE ADC RMS APA2_\"histl\"_none;Channel;RCE ADC RMS",512,1024,1535);
  hADCRMSChannelAPA4                    = new TProfile("RCE_APA3_ADC_RMS_Channel_All","RCE ADC RMS APA3_\"histl\"_none;Channel;RCE ADC RMS",512,1536,2047);
  hADCChannel                           = new TProfile2D("RCE__ADC_Total_Channel_All","ADC vs Channel_\"colz\"_none;Channel;ADC Value",NRCEChannels,0,NRCEChannels,2000,0,2000);
  hAvADCChannelEvent                    = new TH2D("RCE__ADC_Mean_Event_First100","Average RCE ADC Value_\"colz\"_none;Event;Channel",100,0,100,NRCEChannels,0,NRCEChannels);
  hRCEDNoiseChannel                     = new TProfile("RCE__ADC_DNoise_Channel_All","RCE DNoise_\"colz\"_none;Channel;DNoise",NRCEChannels,0,NRCEChannels);
  hTotalADCEvent                        = new TH1I("RCE__ADC_Total__All","Total RCE ADC_\"colz\"_logy;Total ADC;",100,0,1000000000);
  hTotalRCEHitsEvent                    = new TH1I("RCE__Hits_Total__All","Total RCE Hits in Event_\"colz\"_logy;Total RCE Hits;",100,0,10000000);
  hTotalRCEHitsChannel                  = new TH1I("RCE__Hits_Total_Channel_All","Total RCE Hits by Channel_\"colz\"_none;Channel;Total RCE Hits;",NRCEChannels,0,NRCEChannels);
  hNumMicroslicesInMillislice           = new TH1I("RCE__Microslices_Total_Millislice_All","Number of Microslices in Millislice_\"colz\"_none;Millislice;Number of Microslices;",16,100,116);
  hNumNanoslicesInMicroslice            = new TH1I("RCE__Nanoslices_Total_Microslice_All","Number of Nanoslices in Microslice_\"colz\"_logy;Number of Nanoslices;",1000,0,1100);
  hNumNanoslicesInMillislice            = new TH1I("RCE__Nanoslices_Total_Millislice_All","Number of Nanoslices in Millislice_\"colz\"_logy;Number of Nanoslices;",1000,0,11000);
  hTimesADCGoesOverThreshold            = new TH1I("RCE__ADC_TimesOverThreshold__All","Times RCE ADC Over Threshold_\"colz\"_none;Times ADC Goes Over Threshold;",100,0,1000);
  hAsymmetry                            = new TProfile("RCE__ADC_Asymmetry__All","Asymmetry of Bipolar Pulse_\"colz\"_none;Channel;Asymmetry",NRCEChannels,0,NRCEChannels);
  hRCEBitCheckAnd                       = new TH2I("RCE__ADCBit_On_Channel_All","RCE ADC Bits Always On_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  hRCEBitCheckOr                        = new TH2I("RCE__ADCBit_Off_Channel_All","RCE ADC Bits Always Off_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  hLastSixBitsCheckOn                   = new TProfile("RCE__ADCLast6Bits_On_Channel_All","Last Six RCE ADC Bits On_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  hLastSixBitsCheckOff                  = new TProfile("RCE__ADCLast6Bits_Off_Channel_All","Last Six RCE ADC Bits Off_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannelMap[channel]                = new TProfile("RCE_ADCChannel"+TString(std::to_string(channel)),"RCE ADC v Tick for Channel "+TString(std::to_string(channel))+";Tick;ADC;",5000,0,5000);
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice) {
    hAvADCMillislice[millislice]        = new TH1D("RCE_AvADCMillislice"+TString(std::to_string(millislice)),"Av ADC for Millislice "+TString(std::to_string(millislice))+";Event;Av ADC;",10000,0,10000);
    hAvADCMillisliceChannel[millislice] = new TH1D("RCE_AvADCMillisliceChannel"+TString(std::to_string(millislice)),"Av ADC v Channel for Millislice "+TString(std::to_string(millislice))+";Channel;Av ADC;",128,0,128);
  }
  for (std::vector<int>::const_iterator debugchannel = DebugChannels.begin(); debugchannel != DebugChannels.end(); ++debugchannel)
    hDebugChannelHists[(*debugchannel)] = new TH1D("RCE_Channel"+TString(std::to_string(*debugchannel))+"SingleEvent","Channel "+TString(std::to_string(*debugchannel))+" for Single Event",5000,0,5000);

  // SSP hists
  hWaveformMean = new TProfile("SSP__ADC_Mean_Channel_All","SSP ADC Mean_\"histl\"_none;Channel;Average Waveform",NSSPChannels,0,NSSPChannels);
  hWaveformRMS = new TProfile("SSP__ADC_RMS_Channel_All","SSP ADC RMS_\"histl\"_none;Channel;RMS of Waveform",NSSPChannels,0,NSSPChannels);
  hWaveformPeakHeight = new TProfile("SSP__ADC_PeakHeight_Channel_All","Waveform Peak Height_\"histl\"_none;Channel;Peak Height",NSSPChannels,0,NSSPChannels);
  hWaveformIntegral = new TProfile("SSP__ADC_Integral_Channel_All","Waveform Integral_\"histl\"_none;Channel;Integral",NSSPChannels,0,NSSPChannels);
  hWaveformIntegralNorm = new TProfile("SSP__ADC_IntegralNorm_Channel_All","Waveform Integral (normalised by window size)_\"histl\"_none;Channel;Integral",NSSPChannels,0,NSSPChannels);
  hWaveformPedestal = new TProfile("SSP__ADC_Pedestal_Channel_All","Waveform Pedestal_\"histl\"_none;Channel;Pedestal",NSSPChannels,0,NSSPChannels);
  hWaveformNumTicks = new TProfile("SSP__Ticks__Channel_All","Num Ticks in Trigger_\"histl\"_none;Number of Ticks",NSSPChannels,0,NSSPChannels);
  hNumberOfTriggers = new TProfile("SSP__Triggers_Total_Channel_All","Number of Triggers_\"histl\"_none;Channel;Number of Triggers",NSSPChannels,0,NSSPChannels);

  // PTB hists
  hPTBTSUCounterHitRateWU = new TProfile("PTB_TSUWU_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (West Up)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBTSUCounterHitRateWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateWU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeWU = new TProfile("PTB_TSUWU_ActivationTime_Mean_Counter_All","PTB counter average activation time (West Up)_\"\"_none;Counter number; Time",10,1,11);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateEL = new TProfile("PTB_TSUEL_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (East Low)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBTSUCounterHitRateEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateEL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeEL = new TProfile("PTB_TSUEL_ActivationTime_Mean_Counter_All","PTB counter average activation time (East Low)_\"\"_none;Counter number; Time",10,1,11);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateExtra = new TProfile("PTB_TSUExtra_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (Empty counter bits - SHOULD BE EMPTY)_\"\"_none;Counter number; No. hits per millislice",4,1,5);
  hPTBTSUCounterHitRateExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterHitRateExtra->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeExtra = new TProfile("PTB_TSUExtra_ActivationTime_Mean_Counter_All","PTB counter average activation time (Empty counter bits - SHOULD BE EMPTY)_\"\"_none;Counter number; Time",4,1,5);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateNU = new TProfile("PTB_TSUNU_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (North Up)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeNU = new TProfile("PTB_TSUNU_ActivationTime_Mean_Counter_All","PTB counter average activation time (North Up)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateSL = new TProfile("PTB_TSUSL_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (South Low)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeSL = new TProfile("PTB_TSUSL_ActivationTime_Mean_Counter_All","PTB counter average activation time (South Low)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateNL = new TProfile("PTB_TSUNL_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (North Low)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNL->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeNL = new TProfile("PTB_TSUNL_ActivationTime_Mean_Counter_All","PTB counter average activation time (North Low)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->CenterLabels();

  hPTBTSUCounterHitRateSU = new TProfile("PTB_TSUSU_Hits_Mean_Counter_All","PTB TSU counter hit Rate (per millislice) (South Up)_\"\"_none;Counter number; No. hits per millislice",6,1,7);
  hPTBTSUCounterHitRateSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSU->GetXaxis()->CenterLabels();
  hPTBTSUCounterActivationTimeSU = new TProfile("PTB_TSUSU_ActivationTime_Mean_Counter_All","PTB counter average activation time (South Up)_\"\"_none;Counter number; Time",6,1,7);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateRM = new TProfile("PTB_BSURM_Hits_Mean_Counter_All","PTB BSU counter hit Rate (per millislice) (RM)_\"\"_none;Counter number; No. hits per millislice",16,1,17);
  hPTBBSUCounterHitRateRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterHitRateRM->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeRM = new TProfile("PTB_BSURM_ActivationTime_Mean_Counter_All","PTB counter average activation time (RM)_\"\"_none;Counter number; Time",16,1,17);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateCU = new TProfile("PTB_BSUCU_Hits_Mean_Counter_All","PTB BSU counter hit Rate (per millislice) (CU)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBBSUCounterHitRateCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateCU->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeCU = new TProfile("PTB_BSU_ActivationTime_Mean_Counter_All","PTB counter average activation time (CU)_\"\"_none;Counter number; Time",10,1,11);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateCL = new TProfile("PTB_BSUCL_Hits_Mean_Counter_All","PTB BSU counter hit Rate (per millislice) (CL)_\"\"_none;Counter number; No. hits per millislice",13,1,14);
  hPTBBSUCounterHitRateCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterHitRateCL->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeCL = new TProfile("PTB_BSUCL_ActivationTime_Mean_Counter_All","PTB counter average activation time (CL)_\"\"_none;Counter number; Time",13,1,14);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->CenterLabels();

  hPTBBSUCounterHitRateRL = new TProfile("PTB_BSURL_Hits_Mean_Counter_All","PTB BSU counter hit Rate (per millislice) (RL)_\"\"_none;Counter number; No. hits per millislice",10,1,11);
  hPTBBSUCounterHitRateRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateRL->GetXaxis()->CenterLabels();
  hPTBBSUCounterActivationTimeRL = new TProfile("PTB_BSURL_ActivationTime_Mean_Counter_All","PTB counter average activation time (RL)_\"\"_none;Counter number; Time",10,1,11);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->CenterLabels();

  hPTBTriggerRates = new TProfile("PTB__Hits_Mean_MuonTrigger_All","Muon trigger rates_\"\"_none;Muon trigger name; No. hits per millislice",4,1,5);
  hPTBTriggerRates->GetXaxis()->SetBinLabel(1,"BSU RM-CM");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(2,"TSU NU-SL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(3,"TSU SU-NL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(4,"TSU EL-WU");

  // The order the histograms are added will be the order they're displayed on the web!
  fHistArray.Add(hNumSubDetectorsPresent); 
  fHistArray.Add(hSubDetectorsPresent); fHistArray.Add(hSubDetectorsWithData);
  fHistArray.Add(hSizeOfFiles); fHistArray.Add(hSizeOfFilesPerEvent);

  fHistArray.Add(hADCMeanChannelAPA1); fHistArray.Add(hADCMeanChannelAPA2);
  fHistArray.Add(hADCMeanChannelAPA3); fHistArray.Add(hADCMeanChannelAPA4);
  fHistArray.Add(hADCRMSChannelAPA1); fHistArray.Add(hADCRMSChannelAPA2);
  fHistArray.Add(hADCRMSChannelAPA3); fHistArray.Add(hADCRMSChannelAPA4);
  fHistArray.Add(hADCChannel);
  fHistArray.Add(hRCEDNoiseChannel);
  fHistArray.Add(hAvADCChannelEvent); fHistArray.Add(hTotalRCEHitsChannel);
  fHistArray.Add(hTotalADCEvent); fHistArray.Add(hTotalRCEHitsEvent);
  fHistArray.Add(hAsymmetry); fHistArray.Add(hTimesADCGoesOverThreshold);
  fHistArray.Add(hRCEBitCheckAnd); fHistArray.Add(hRCEBitCheckOr);
  fHistArray.Add(hLastSixBitsCheckOn); fHistArray.Add(hLastSixBitsCheckOff);
  fHistArray.Add(hNumMicroslicesInMillislice); 

  fHistArray.Add(hWaveformMean);
  fHistArray.Add(hWaveformRMS);
  fHistArray.Add(hWaveformPeakHeight);
  fHistArray.Add(hWaveformIntegral);
  fHistArray.Add(hWaveformIntegralNorm);
  fHistArray.Add(hWaveformPedestal);
  fHistArray.Add(hWaveformNumTicks);
  fHistArray.Add(hNumberOfTriggers);

  fHistArray.Add(hPTBTriggerRates);
  fHistArray.Add(hPTBTSUCounterActivationTimeWU); fHistArray.Add(hPTBTSUCounterHitRateWU);
  fHistArray.Add(hPTBTSUCounterActivationTimeEL); fHistArray.Add(hPTBTSUCounterHitRateEL);
  fHistArray.Add(hPTBTSUCounterActivationTimeExtra); fHistArray.Add(hPTBTSUCounterHitRateExtra);
  fHistArray.Add(hPTBTSUCounterActivationTimeNU); fHistArray.Add(hPTBTSUCounterHitRateNU);
  fHistArray.Add(hPTBTSUCounterActivationTimeSL); fHistArray.Add(hPTBTSUCounterHitRateSL);
  fHistArray.Add(hPTBTSUCounterActivationTimeNL); fHistArray.Add(hPTBTSUCounterHitRateNL);
  fHistArray.Add(hPTBTSUCounterActivationTimeSU); fHistArray.Add(hPTBTSUCounterHitRateSU);
  fHistArray.Add(hPTBBSUCounterActivationTimeRM); fHistArray.Add(hPTBBSUCounterHitRateRM);
  fHistArray.Add(hPTBBSUCounterActivationTimeCU); fHistArray.Add(hPTBBSUCounterHitRateCU);
  fHistArray.Add(hPTBBSUCounterActivationTimeCL); fHistArray.Add(hPTBBSUCounterHitRateCL);
  fHistArray.Add(hPTBBSUCounterActivationTimeRL); fHistArray.Add(hPTBBSUCounterHitRateRL);

  // General captions
  fFigureCaptions["General__NumberOfSubdetectors_Total__All"] = "Number of subdetectors present in each event in the data (one entry per event)";
  fFigureCaptions["General__Last20Files_Size_RunSubrun_All"] = "Size of the data files made by the DAQ for the last 20 runs";
  fFigureCaptions["General__Last20Files_SizePerEvent_RunSubrun_All"] = "Size of event in each of the last 20 data files made by the DAQ (size of file / number of events in file)";
  fFigureCaptions["General__SubdetectorsWithData_Present__All"] = "Subdetectors with data in run";
  fFigureCaptions["General__SubdetectorsPresent_Total__All"] = "Subdetectors with data per event";

  // RCE captions
  fFigureCaptions["RCE_APA0_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA1_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA2_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA3_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA0_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA1_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA2_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE_APA3_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  fFigureCaptions["RCE__ADC_Total_Channel_All"] = "ADC value for each channel (one entry per tick); averaged over all events";
  fFigureCaptions["RCE__ADC_DNoise_Channel_All"] = "DNoise (difference in ADC between neighbouring channels for the same tick) for the RCE ADCs (profiled over all events read)";
  fFigureCaptions["RCE__ADC_Mean_Event_First100"] = "Average RCE ADC across a channel for an event, shown for the first 100 events";
  fFigureCaptions["RCE__Hits_Total_Channel_All"] = "Total number of RCE hits in each channel across all events";
  fFigureCaptions["RCE__ADC_Total__All"] = "Total RCE ADC recorded for an event across all channels (one entry per event)";
  fFigureCaptions["RCE__Hits_Total__All"] = "Total number of hits recorded on the RCEs per event (one entry per event)";
  fFigureCaptions["RCE__ADC_Asymmetry__All"] = "Asymmetry of the bipolar pulse measured on the induction planes, by channel (profiled across all events). Zero means completely symmetric pulse";
  fFigureCaptions["RCE__ADC_TimesOverThreshold__All"] = "Number of times an RCE hit goes over a set ADC threshold";
  fFigureCaptions["RCE__ADCBit_On_Channel_All"] = "Check for stuck RCE bits: bits which are always on";
  fFigureCaptions["RCE__ADCBit_Off_Channel_All"] = "Check for stuck RCE bits: bits which are always off";
  fFigureCaptions["RCE__ADCLast6Bits_On_Channel_All"] = "Fraction of all RCE ADC values with the last six bits stuck on (profiled; one entry per ADC)";
  fFigureCaptions["RCE__ADCLast6Bits_Off_Channel_All"] = "Fraction of all RCE ADC values with the last six bits stuck off (profiled; one entry per ADC)";
  fFigureCaptions["RCE__Microslices_Total_Millislice_All"] = "Number of microslices in a millislice in this run";

  // SSP captions
  fFigureCaptions["SSP__ADC_Mean_Channel_All"] = "Average waveform across SSP channels (profiled over all events)";
  fFigureCaptions["SSP__ADC_RMS_Channel_All"] = "RMS of the SSP waveforms (profiled across all events)";
  fFigureCaptions["SSP__ADC_PeakHeight_Channel_All"] = "Peak height of the SSP waveforms (profiled across all events)";
  fFigureCaptions["SSP__ADC_Integral_Channel_All"] = "Integral of the SSP waveforms (profiled across all events)";
  fFigureCaptions["SSP__ADC_IntegralNorm_Channel_All"] = "Normalised integral of the SSP waveforms (profiled across all events)";
  fFigureCaptions["SSP__ADC_Pedestal_Channel_All"] = "Pedestal of the SSP waveforms (profiled across all events)";
  fFigureCaptions["SSP__Ticks__Channel_All"] = "Number of ticks in each trigger";
  fFigureCaptions["SSP__Triggers_Total_Channel_All"] = "Total number of triggers per channel (profiled across all events)";

  // PTB captions
  fFigureCaptions["PTB_TSUWU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU West Up counters";
  fFigureCaptions["PTB_TSUWU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU West Up counters";
  fFigureCaptions["PTB_TSUEL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU East Low counters";
  fFigureCaptions["PTB_TSUEL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU East Low counters";
  fFigureCaptions["PTB_TSUExtra_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU extra counter bits (SHOULD BE EMPTY)";
  fFigureCaptions["PTB_TSUExtra_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU extra counter bits (SHOULD BE EMPTY)";
  fFigureCaptions["PTB_TSUNU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Up counters";
  fFigureCaptions["PTB_TSUNU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Up counters";
  fFigureCaptions["PTB_TSUSL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU South Low counters";
  fFigureCaptions["PTB_TSUSL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU South Low counters";
  fFigureCaptions["PTB_TSUNL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Low counters";
  fFigureCaptions["PTB_TSUNL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Low counters";
  fFigureCaptions["PTB_TSUSU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Low counters";
  fFigureCaptions["PTB_TSUSU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Low counters";
  fFigureCaptions["PTB_BSURM_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU RM counters";
  fFigureCaptions["PTB_BSURM_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU RM counters";
  fFigureCaptions["PTB_BSUCU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU CU counters";
  fFigureCaptions["PTB_BSU_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU CU counters";
  fFigureCaptions["PTB_BSUCL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU CL counters";
  fFigureCaptions["PTB_BSUCL_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU CL counters";
  fFigureCaptions["PTB_BSURL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU RL counters";
  fFigureCaptions["PTB_BSURL_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU RL counters";
  fFigureCaptions["PTB__Hits_Mean_MuonTrigger_All"] = "Average hit rates per millislice of the muon trigger system";

}
