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

void OnlineMonitoring::MonitoringData::BeginMonitoring(int run, int subrun, TString const& monitorSavePath, bool detailedMonitoring, bool scopeMonitoring) {

  /// Sets up monitoring for new subrun

  // Set up new subrun
  fHistArray.Clear();
  fCanvas = new TCanvas("canv","",800,600);
  fFilledRunData = false;
  fDetailedMonitoring = detailedMonitoring;

  // Get start time of run
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[80];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer,80,"%A %B %d, %R",timeinfo);
  fRunStartTime = std::string(buffer);

  // Get directory for this run
  std::ostringstream directory;
  directory << monitorSavePath << "Run" << run << "Subrun" << subrun << "/";
  fHistSaveDirectory = directory.str();

  // Make the directories to save the files
  std::ostringstream cmd;
  cmd << "touch " << directory.str() << "; rm -rf " << directory.str() << "; mkdir " << directory.str()
      << "; mkdir " << directory.str() << "General"
      << "; mkdir " << directory.str() << "RCE"
      << "; mkdir " << directory.str() << "SSP"
      << "; mkdir " << directory.str() << "PTB";
  system(cmd.str().c_str());

  // Outfile
  fDataFile = new TFile(fHistSaveDirectory+TString("monitoringRun"+std::to_string(run)+"Subrun"+std::to_string(subrun)+".root"),"RECREATE");
  fDataFile->mkdir("General");
  fDataFile->mkdir("RCE");
  fDataFile->mkdir("SSP");
  fDataFile->mkdir("PTB");

  // Tree
  if (fDetailedMonitoring) {
    fDataTree = new TTree("RawData","Raw Data");
    fDataTree->Branch("RCE",&fRCEADC);
    fDataTree->Branch("SSP",&fSSPADC);
  }

  // Monitoring in scope mode
  if (scopeMonitoring)
    this->MakeScopeHistograms();

  // Monitoring in normal mode
  else {
    this->MakeHistograms();
    if (detailedMonitoring)
      this->MakeDetailedHistograms();
  }

  for (unsigned int sspChan = 0; sspChan < NSSPChannels; ++sspChan) {
    fNSSPFragments.push_back(0);
    fNSSPTriggers.push_back(0);
  }

}

void OnlineMonitoring::MonitoringData::EndMonitoring() {

  /// Clear up after writing out the monitoring data

  // Free the memory for the histograms
  fHistArray.Delete();

  // Free up all used memory
  if (fDetailedMonitoring)
    for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
      hADCChannelMap.at(channel)->Delete();

  fDataFile->Close();
  if (fDetailedMonitoring)
    delete fDataTree;
  delete fDataFile;
  delete fCanvas;

}

void OnlineMonitoring::MonitoringData::FillBeforeWriting() {

  /// Fills all data objects which require filling just before writing out

  for (unsigned int sspChan = 0; sspChan < NSSPChannels; ++sspChan)
    hSSPTriggerRate->Fill(sspChan, fNSSPTriggers.at(sspChan)/(double)fNSSPFragments.at(sspChan));

}

void OnlineMonitoring::MonitoringData::FillTree(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter) {
  fRCEADC = rceformatter.ADCVector();
  // fSSPADC = sspformatter.ADCVector();
  std::cout << "Number of SSPs is " << sspformatter.NumSSPs << std::endl;
  fDataTree->Fill();
}

void OnlineMonitoring::MonitoringData::GeneralMonitoring(RCEFormatter const& rceformatter,
							 SSPFormatter const& sspformatter,
							 PTBFormatter const& ptbformatter,
							 TString const& dataDirPath) {

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
  if (!fFilledRunData) {
    fFilledRunData = true;

    // Size of files
    std::multimap<Long_t,std::pair<std::vector<TString>,Long_t>,std::greater<Long_t> > fileMap;
    TSystemDirectory dataDir("dataDir",dataDirPath);
    const TList *files = dataDir.GetListOfFiles();
    if (files) {
      TSystemFile *file;
      TString fileName;
      Long_t id,modified,size,flags;
      TIter next(files);
      while ( (file = (TSystemFile*)next()) ) {
	fileName = file->GetName();
    	if ( !file->IsDirectory() && fileName.EndsWith(".root") && !fileName.BeginsWith("lbne_r-") ) {
	  const TString path = dataDirPath+TString(file->GetName());
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

  // Timing sync plots

  // SSP

  long double min_average_time = 0;
  long double max_average_time = 0;
  int NChannelsWithTrigger = 0, NSSPTriggers = 0, NSSPChannelsPerSSP = NSSPChannels/NSSPs;
  long double average_ssp_trigger_time = 0;
  std::map<int,long double> average_ssp_times;
  for (unsigned int ssp = 0; ssp < NSSPs; ++ssp)
    average_ssp_times[ssp] = 0.;

  // Loop over the triggers in each channel
  const std::map<int,std::vector<Trigger> > channelTriggers = sspformatter.ChannelTriggers();
  for (std::map<int,std::vector<Trigger> >::const_iterator channelIt = channelTriggers.begin(); channelIt != channelTriggers.end(); ++channelIt) {

    const std::vector<Trigger> triggers = channelIt->second;

    // Don't do arithmetic when there is no arithmetic to do
    if (triggers.size() == 0)
      continue;

    ++NChannelsWithTrigger;
    NSSPTriggers += triggers.size();

    // Find the average trigger time for this channel
    long double average_channel_time = 0;
    for (std::vector<Trigger>::const_iterator triggerIt = triggers.begin(); triggerIt != triggers.end(); ++triggerIt) {
      average_channel_time += triggerIt->Timestamp;
      average_ssp_trigger_time += triggerIt->Timestamp;
    }
    average_channel_time /= triggers.size();

    // Find if this is the first channel for an SSP
    // If so, store the average_channel_time as both the min and max
    if (std::distance(channelTriggers.begin(),channelIt) % NSSPChannelsPerSSP == 0) {
      min_average_time = average_channel_time;
      max_average_time = average_channel_time;
    }
    // Otherwise make comparisons
    else {
      min_average_time = std::min(min_average_time,average_channel_time);
      max_average_time = std::max(max_average_time,average_channel_time);
    }

    // See if we've reached the end of an SSP
    if (std::distance(channelTriggers.begin(),channelIt) % NSSPChannelsPerSSP == NSSPChannelsPerSSP-1) {

      // Only one trigger
      if (NChannelsWithTrigger <= 1) {
	NChannelsWithTrigger = 0;
	std::cout << "Found SSP with only one trigger: continuing..." << std::endl;
	continue;
      }

      NChannelsWithTrigger = 0;

      // Add a plot on the graph for the SSP we've just finished looking at
      int plot_index = std::distance(channelTriggers.begin(), channelIt) / NSSPChannelsPerSSP;
      //hTimeSyncSSPs[plot_index]->SetPoint(hTimeSyncSSPs[plot_index]->GetN(),min_average_time,max_average_time-min_average_time);

      // Store average SSP time
      average_ssp_trigger_time /= NSSPTriggers;
      average_ssp_times[plot_index] = average_ssp_trigger_time;

      NSSPTriggers = 0;
      average_ssp_trigger_time = 0;

    }

  }

  // Average of all SSP times
  long double average_of_average_ssp_times = 0;
  unsigned int NSSPsWithTriggers = 0;
  for (std::map<int,long double>::iterator sspIt = average_ssp_times.begin(); sspIt != average_ssp_times.end(); ++sspIt)
    if (sspIt->second > 0) {
      average_of_average_ssp_times += sspIt->second;
      ++NSSPsWithTriggers;
    }
  if (NSSPsWithTriggers)
    average_of_average_ssp_times /= NSSPsWithTriggers;
  for (std::map<int,long double>::iterator sspIt = average_ssp_times.begin(); sspIt != average_ssp_times.end(); ++sspIt)
    if (sspIt->second > 0) {
      // int plot_index = std::distance(average_ssp_times.begin(), sspIt);
      // hTimeSyncAverageSSPs[plot_index]->SetPoint(hTimeSyncAverageSSPs[plot_index]->GetN(),average_of_average_ssp_times,sspIt->second-average_of_average_ssp_times);
    }

}

void OnlineMonitoring::MonitoringData::RCEMonitoring(RCEFormatter const& rceformatter, int event) {

  /// Fills all histograms pertaining to RCE hardware monitoring

  // NEED TO SORT THIS OUT! USE CHANNEL MAP TO GET THE INDUCTION CHANNELS
  bool isInduction = true;

  // Variables for event
  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();
  const std::vector<std::vector<unsigned long> > timestamps = rceformatter.TimestampVector();
  int totalADC = 0, totalRCEHitsEvent = 0, timesADCGoesOverThreshold = 0;

  // Highest number of ticks across all channels
  unsigned int maxNumTicks = (*std::max_element(ADCs.begin(), ADCs.end(), [](std::vector<int> A, std::vector<int> B) { return A.size() < B.size(); })).size();

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {

    if (!ADCs.at(channel).size())
      continue;

    // std::vector<int>::const_iterator max_it = std::max_element(ADCs.at(channel).begin(),ADCs.at(channel).end());
    // int index = std::distance(ADCs.at(channel).begin(), max_it);
    // std::cout<<"RCE: " << channel/NChannelsPerRCE <<" Channel: " << channel << "  max_ADC: " << (*max_it) << "  timestamp: " << timestamps.at(channel).at(index) << std::endl;

    // Variables for channel
    bool peak = false;
    int totalRCEHitsChannel = 0;
    bool bitCheckAnd = 0xFFFF, bitCheckOr = 0;
    double asymmetry = 0;
    double ADCsum = 0, ADCdiff = 0;

    // Find the mean and RMS of ADCs for this channel
    double mean = TMath::Mean(ADCs.at(channel).begin(),ADCs.at(channel).end());
    double rms  = TMath::RMS (ADCs.at(channel).begin(),ADCs.at(channel).end());

    // Make a vector of DNoise!
    // DNoise is the difference in ADC between two neighbouring channels for the same tick
    std::vector<int> dNoise;

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {

      int ADC = ADCs.at(channel).at(tick);

      // Fill hists for tick
      if (fDetailedMonitoring)
	hADCChannelMap.at(channel)->Fill(tick,ADC);
      if (channel and !ADCs.at(channel-1).empty() and tick < ADCs.at(channel-1).size())
	dNoise.push_back(ADC-ADCs.at(channel-1).at(tick));
      hADCChannel->Fill(channel,ADC,1);

      // Increase variables
      double threshold = 10;
      totalADC += ADC;
      if (ADC > threshold) {
	++totalRCEHitsChannel;
	++totalRCEHitsEvent;
      }

      // Times over threshold
      if ( (ADC > threshold) and !peak ) {
	++timesADCGoesOverThreshold;
	peak = true;
      }
      if ( tick and (ADC < ADCs.at(channel).at(tick-1)) and peak ) peak = false;

      // Bit check
      bitCheckAnd &= ADC;
      bitCheckOr  |= ADC;

      // Check last 6 bits
      int mask1 = 0xFFC0, mask2 = 0x003F;
      hLastSixBitsCheckOff->Fill(channel,((ADC & mask1) == ADC));
      hLastSixBitsCheckOn ->Fill(channel,((ADC & mask2) == ADC));

    }

    // Find the RMS of the DNoise
    double dNoiseRMS = TMath::RMS(dNoise.begin(), dNoise.end()) / TMath::Sqrt(2);

    // Fill hists for channel
    hADCMeanChannelAPA1->Fill(channel,mean);
    hADCMeanChannelAPA2->Fill(channel,mean);
    hADCMeanChannelAPA3->Fill(channel,mean);
    hADCMeanChannelAPA4->Fill(channel,mean);
    hADCRMSChannelAPA1->Fill(channel,rms);
    hADCRMSChannelAPA2->Fill(channel,rms);
    hADCRMSChannelAPA3->Fill(channel,rms);
    hADCRMSChannelAPA4->Fill(channel,rms);
    hADCDNoiseRMSChannelAPA1->Fill(channel,dNoiseRMS);
    hADCDNoiseRMSChannelAPA2->Fill(channel,dNoiseRMS);
    hADCDNoiseRMSChannelAPA3->Fill(channel,dNoiseRMS);
    hADCDNoiseRMSChannelAPA4->Fill(channel,dNoiseRMS);
    hAvADCChannelEvent->Fill(event,channel,mean);
    hTotalRCEHitsChannel->Fill(channel+1,totalRCEHitsChannel);
    int tbit = 1;
    for (int bitIt = 0; bitIt < 16; ++bitIt) {
      hRCEBitCheckAnd->Fill(channel,bitIt,(bitCheckAnd & tbit));
      hRCEBitCheckOr ->Fill(channel,bitIt,(bitCheckOr & tbit));
      tbit <<= 1;
    }

    // Get tick ratio
    double tickRatio = ADCs.at(channel).size() / (double)maxNumTicks;
    hTickRatioChannel->Fill(channel,tickRatio);

    // Loop over blocks to look at the asymmetry
    for (int block = 0; block < rceformatter.NumBlocks().at(channel); ++block) {
      // Loop over the ticks within the block
      for (int tick = rceformatter.BlockBegin().at(channel).at(block); tick < rceformatter.BlockBegin().at(channel).at(block)+rceformatter.BlockSize().at(channel).at(block); ++tick) {
	if (isInduction) {
	  ADCdiff += ADCs.at(channel).at(tick);
	  ADCsum += abs(ADCs.at(channel).at(tick));
	}
      } // End of tick loop
    } // End of block loop

    if (isInduction && ADCsum) asymmetry = (double)ADCdiff / (double)ADCsum;
    hAsymmetry->Fill(channel+1,asymmetry);

  }

  // Fill hists for event
  hTotalADCEvent            ->Fill(totalADC);
  hTotalRCEHitsEvent        ->Fill(totalRCEHitsEvent);
  hTimesADCGoesOverThreshold->Fill(timesADCGoesOverThreshold);

  // Find average ADCs for each millislice
  if (fDetailedMonitoring) {
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
      hAvADCMillislice.at(millislice)->Fill(event, mean);

    }
  }

}

void OnlineMonitoring::MonitoringData::RCELessFrequentMonitoring(RCEFormatter const& rceformatter) {

  /// Intended to be called less frequently than the normal RCEMonitoring method and fill bigger histograms

  const std::vector<std::vector<int> >& ADCs = rceformatter.ADCVector();

  // FFT
  int numBins = 1000;
  TH1F* hData = new TH1F("hData","",numBins,0,numBins*SamplingPeriod);
  TH1F* hFFTData = new TH1F("hFFTData","",numBins,0,numBins);
  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
    if (!ADCs.at(channel).size())
      continue;
    hData->Reset();
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick)
      hData->SetBinContent(tick+1,ADCs.at(channel).at(tick));
    hData->FFT(hFFTData,"MAG");
    for (int bin = 1; bin < hFFTData->GetNbinsX(); ++bin){
      double frequency = 2. * bin / (double) hFFTData->GetNbinsX();
      hFFTChannelRCE00->Fill(channel,frequency,hFFTData->GetBinContent(bin+1));
    }
  }

}

void OnlineMonitoring::MonitoringData::RCEScopeMonitoring(RCEFormatter const& rceformatter, int event) {

  /// Fills all data objects with RCE monitoring when running the DAQ in scope mode

  int upperTime = 1;

  // Find the time since the start of the run
  double time = event / (double)EventRate;
  double tickTime = time;

  if (time > upperTime)
    return;

  // Get the ADCs and the scope channel
  const std::vector<std::vector<int> >& ADCs = rceformatter.ADCVector();
  const int scopeChannel = rceformatter.ScopeChannel();

  hScopeTrace1s->SetTitle(("RCE Scope Trace (Channel "+std::to_string(scopeChannel)+")_\"hist\"_none;Time (s);ADC").c_str());
  hScopeTraceFFT1s->SetTitle(("RCE FFT Scope Trace (Channel "+std::to_string(scopeChannel)+")_\"hist\"_none;Freq (Hz);").c_str());

  // Sanity check -- shouldn't be more than one channel!
  if (ADCs.size() > 1) {
    mf::LogError("Monitoring") << "Running monitoring in scope mode -- more than one channel present!";
    return;
  }

  // Fill the trace histogram
  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
      tickTime = time + (tick / EventRate);
      hScopeTrace1s->Fill(tickTime,ADCs.at(channel).at(tick));
    }
  }

  // FFT
  if (tickTime > upperTime)
    hScopeTrace1s->FFT(hScopeTraceFFT1s,"MAG");

}

void OnlineMonitoring::MonitoringData::SSPMonitoring(SSPFormatter const& sspformatter) {

  /// Fills all histograms pertaining to SSP hardware monitoring

  const std::map<int,std::vector<Trigger> > channelTriggers = sspformatter.ChannelTriggers();

  // Loop over channels
  for (std::map<int,std::vector<Trigger> >::const_iterator channelIt = channelTriggers.begin(); channelIt != channelTriggers.end(); ++channelIt) {

    const int channel = channelIt->first;
    const std::vector<Trigger> triggers = channelIt->second;
    ++fNSSPFragments.at(channel);
    fNSSPTriggers.at(channel) += triggers.size();

    // Loop over triggers
    for (std::vector<Trigger>::const_iterator triggerIt = triggers.begin(); triggerIt != triggers.end(); ++triggerIt) {

      if (triggerIt->NTicks == 0)
	continue;

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
    hTriggerFraction->Fill(channel,triggers.size());

  }

  // Fill event level hists

}

#ifdef OLD_CODE
void OnlineMonitoring::MonitoringData::PTBMonitoring(PTBFormatter const& ptb_formatter) {

  /// Produces PTB monitoring histograms

  if (ptb_formatter.Payloads().size() == 0)
    return;

  // Fill the payload hists
  const std::vector<unsigned int> payloads = ptb_formatter.Payloads();
  // NFB: Careful that with all counters on we might run into pretty large payload numbers (in the hundreds)
  //      Be sure that the range is large enough.
  hPTBBlockLength->Fill(payloads.size());
  for (std::vector<unsigned int>::const_iterator payloadIt = payloads.begin(); payloadIt != payloads.end(); ++payloadIt) {
    if (*payloadIt == 1)      hPTBPayloadType->Fill("Counter",1);
    else if (*payloadIt == 2) hPTBPayloadType->Fill("Trigger",1);
    else if (*payloadIt == 4) hPTBPayloadType->Fill("Checksum",1);
    else if (*payloadIt == 7) hPTBPayloadType->Fill("Timestamp",1);
    else if (*payloadIt == 0) hPTBPayloadType->Fill("Self-test",1);
    else mf::LogWarning("Monitoring") << "Warning: payload type not recognised";
  }

  // Fill the counter hists
  unsigned long activation_time = 0;
  double hit_rate = 0;
  int counterNumber = 0;

  //NFB: This logic is completely insane, specially when combining with the
  // original logic of OnlineMonitoring::PTBFormatter::AnalyzeCounter
  for (int i = 0; i < 97; ++i) {

    ptb_formatter.AnalyzeCounter(i,activation_time,hit_rate);

    if (i < 7 and (activation_time != 0 or hit_rate != 0))
      mf::LogError("Monitoring") << "A bit in the PTB payload below bit 7 is set!  Something has gone wrong...";

    // Fill the relevant histograms for this counter
    if (std::find(CounterPos::TSUWU.begin(),CounterPos::TSUWU.end(),i) != CounterPos::TSUWU.end()) {
      counterNumber = std::distance(CounterPos::TSUWU.begin(),std::find(CounterPos::TSUWU.begin(),CounterPos::TSUWU.end(),i)) + 1;
      hPTBTSUCounterHitRateWU->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeWU->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUEL.begin(),CounterPos::TSUEL.end(),i) != CounterPos::TSUEL.end()) {
      counterNumber = std::distance(CounterPos::TSUEL.begin(),std::find(CounterPos::TSUEL.begin(),CounterPos::TSUEL.end(),i)) + 1;
      hPTBTSUCounterHitRateEL->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeEL->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUExtra.begin(),CounterPos::TSUExtra.end(),i) != CounterPos::TSUExtra.end()) {
      counterNumber = std::distance(CounterPos::TSUExtra.begin(),std::find(CounterPos::TSUExtra.begin(),CounterPos::TSUExtra.end(),i)) + 1;
      hPTBTSUCounterHitRateExtra->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeExtra->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUNU.begin(),CounterPos::TSUNU.end(),i) != CounterPos::TSUNU.end()) {
      counterNumber = std::distance(CounterPos::TSUNU.begin(),std::find(CounterPos::TSUNU.begin(),CounterPos::TSUNU.end(),i)) + 1;
      hPTBTSUCounterHitRateNU->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeNU->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUSL.begin(),CounterPos::TSUSL.end(),i) != CounterPos::TSUSL.end()) {
      counterNumber = std::distance(CounterPos::TSUSL.begin(),std::find(CounterPos::TSUSL.begin(),CounterPos::TSUSL.end(),i)) + 1;
      hPTBTSUCounterHitRateSL->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeSL->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUNL.begin(),CounterPos::TSUNL.end(),i) != CounterPos::TSUNL.end()) {
      counterNumber = std::distance(CounterPos::TSUNL.begin(),std::find(CounterPos::TSUNL.begin(),CounterPos::TSUNL.end(),i)) + 1;
      hPTBTSUCounterHitRateNL->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeNL->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::TSUSU.begin(),CounterPos::TSUSU.end(),i) != CounterPos::TSUSU.end()) {
      counterNumber = std::distance(CounterPos::TSUSU.begin(),std::find(CounterPos::TSUSU.begin(),CounterPos::TSUSU.end(),i)) + 1;
      hPTBTSUCounterHitRateSU->Fill(counterNumber,hit_rate);
      hPTBTSUCounterActivationTimeSU->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::BSURM.begin(),CounterPos::BSURM.end(),i) != CounterPos::BSURM.end()) {
      counterNumber = std::distance(CounterPos::BSURM.begin(),std::find(CounterPos::BSURM.begin(),CounterPos::BSURM.end(),i)) + 1;
      hPTBBSUCounterHitRateRM->Fill(counterNumber,hit_rate);
      hPTBBSUCounterActivationTimeRM->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::BSUCU.begin(),CounterPos::BSUCU.end(),i) != CounterPos::BSUCU.end()) {
      counterNumber = std::distance(CounterPos::BSUCU.begin(),std::find(CounterPos::BSUCU.begin(),CounterPos::BSUCU.end(),i)) + 1;
      hPTBBSUCounterHitRateCU->Fill(counterNumber,hit_rate);
      hPTBBSUCounterActivationTimeCU->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::BSUCL.begin(),CounterPos::BSUCL.end(),i) != CounterPos::BSUCL.end()) {
      counterNumber = std::distance(CounterPos::BSUCL.begin(),std::find(CounterPos::BSUCL.begin(),CounterPos::BSUCL.end(),i)) + 1;
      hPTBBSUCounterHitRateCL->Fill(counterNumber,hit_rate);
      hPTBBSUCounterActivationTimeCL->Fill(counterNumber,activation_time);
    }
    else if (std::find(CounterPos::BSURL.begin(),CounterPos::BSURL.end(),i) != CounterPos::BSURL.end()) {
      counterNumber = std::distance(CounterPos::BSURL.begin(),std::find(CounterPos::BSURL.begin(),CounterPos::BSURL.end(),i)) + 1;
      hPTBBSUCounterHitRateRL->Fill(counterNumber,hit_rate);
      hPTBBSUCounterActivationTimeRL->Fill(counterNumber,activation_time);
    }

    // Fill the trigger hists
    double trigger_rate = 0;
    for (int trigger_index = 1; trigger_index <= 4; ++trigger_index){
      ptb_formatter.AnalyzeMuonTrigger(TMath::Power(2,trigger_index-1),trigger_rate);
      ptb_formatter.AnalyzeCalibrationTrigger(TMath::Power(2,trigger_index-1),trigger_rate);
      hPTBTriggerRates->Fill(trigger_index,trigger_rate);
      hPTBTriggerRates->Fill(9-(trigger_index-1),trigger_rate);
    }
    ptb_formatter.AnalyzeSSPTrigger(trigger_rate);
    hPTBTriggerRates->Fill("SSP",trigger_rate);

  }

  return;

}
#else




/////////////// Nuno's code /////////////////////////

void OnlineMonitoring::MonitoringData::PTBMonitoring(PTBFormatter const& ptb_formatter) {

  /// Produces PTB monitoring histograms

  if (ptb_formatter.Payloads().size() == 0)
    return;

  // Fill the payload hists
  const std::vector<lbne::PennMicroSlice::Payload_Header::data_packet_type_t> payloads = ptb_formatter.Payloads();
  hPTBBlockLength->Fill(payloads.size());
  for (std::vector<lbne::PennMicroSlice::Payload_Header::data_packet_type_t>::const_iterator payloadIt = payloads.begin(); payloadIt != payloads.end(); ++payloadIt) {
    if (*payloadIt == lbne::PennMicroSlice::DataTypeCounter) hPTBPayloadType->Fill("Counter",1);
    else if (*payloadIt == lbne::PennMicroSlice::DataTypeTrigger) hPTBPayloadType->Fill("Trigger",1);
    else if (*payloadIt == lbne::PennMicroSlice::DataTypeTimestamp) hPTBPayloadType->Fill("Timestamp",1);
    else if (*payloadIt == lbne::PennMicroSlice::DataTypeWarning)   hPTBPayloadType->Fill("Warning",1);
    else if (*payloadIt == lbne::PennMicroSlice::DataTypeChecksum) hPTBPayloadType->Fill("Checksum",1);
    else hPTBPayloadType->Fill("Other",1);
  }

  // Fill the counter hists
  lbne::PennMicroSlice::Payload_Timestamp::timestamp_t activation_time = 0;
  double hit_rate = 0;
  uint32_t counterNumber = 0;

  // There are a total of 97 counters (even though not all will be on)
  for (uint32_t i = 0; i <= 97; ++i) {

    ptb_formatter.AnalyzeCounter(i,activation_time,hit_rate);

    // Now the structures are filled according to the group they belong to
    // This call is independent of the counter type; always returns the index inside its own type
    counterNumber = lbne::PennMicroSlice::Payload_Counter::get_counter_type_pos(i);

    switch (lbne::PennMicroSlice::Payload_Counter::get_counter_type(i)) {

    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_wu:
      hPTBTSUCounterHitRateWU->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeWU->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_el:
      hPTBTSUCounterHitRateEL->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeEL->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_extra:
      hPTBTSUCounterHitRateExtra->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeExtra->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_nu:
      hPTBTSUCounterHitRateNU->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeNU->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_sl:
      hPTBTSUCounterHitRateSL->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeSL->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_nl:
      hPTBTSUCounterHitRateNL->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeNL->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_tsu_su:
      hPTBTSUCounterHitRateSU->Fill(counterNumber+1,hit_rate);
      hPTBTSUCounterActivationTimeSU->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_bsu_rm:
      hPTBBSUCounterHitRateRM->Fill(counterNumber+1,hit_rate);
      hPTBBSUCounterActivationTimeRM->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_bsu_cu:
      hPTBBSUCounterHitRateCU->Fill(counterNumber+1,hit_rate);
      hPTBBSUCounterActivationTimeCU->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_bsu_cl:
      hPTBBSUCounterHitRateCL->Fill(counterNumber+1,hit_rate);
      hPTBBSUCounterActivationTimeCL->Fill(counterNumber+1,activation_time);
      break;
    case lbne::PennMicroSlice::Payload_Counter::counter_type_bsu_rl:
      hPTBBSUCounterHitRateRL->Fill(counterNumber+1,hit_rate);
      hPTBBSUCounterActivationTimeRL->Fill(counterNumber+1,activation_time);
      break;
    default:
      break;
    }

  }

  // Fill the trigger hists
  for (uint32_t i = 0; i < 4; ++i ) {
    hPTBTriggerRates->Fill(i+1, ptb_formatter.AnalyzeMuonTrigger(PTBTrigger::Muon.at(i)));
    hPTBTriggerRates->Fill(9-i,ptb_formatter.AnalyzeCalibrationTrigger(PTBTrigger::Calibration.at(i)));
  }
  hPTBTriggerRates->Fill("SSP", ptb_formatter.AnalyzeSSPTrigger());

  return;

}

#endif /*OLD_CODE*/

void OnlineMonitoring::MonitoringData::WriteMonitoringData(int run, int subrun, int eventsProcessed, TString const& imageType) {

  /// Writes all the monitoring data currently saved in the data objects

  this->FillBeforeWriting();

  // Get the time we're writing the data out
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[80];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer,80,"%A %B %d, %R",timeinfo);
  std::string writeOutTime(buffer);

  // Make the html for the web pages
  ofstream mainHTML((fHistSaveDirectory+TString("index.html").Data()));
  std::map<std::string,std::unique_ptr<ofstream> > componentHTML;
  mainHTML << "<head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../style/style.css\"><title>35t: Run " << run << ", Subrun " << subrun <<"</title></head>" << std::endl << "<body><a href=\"http://lbne-dqm.fnal.gov\">" << std::endl << "  <div class=\"bannertop\"></div>" << std::endl << "</a>" << std::endl;
  mainHTML << "<h1 align=center>Monitoring for Run " << run << ", Subrun " << subrun << "</h1>" << std::endl;
  mainHTML << "<center>Run started " << fRunStartTime << "; monitoring last updated " << writeOutTime << " (" << eventsProcessed << " events processed)</center></br>" << std::endl;
  for (auto& component : {"General","RCE","SSP","PTB"}) {
    mainHTML << "</br><a href=\"" << component << "\">" << component << "</a>" << std::endl;
    componentHTML[component].reset(new ofstream((fHistSaveDirectory+component+TString("/index.html")).Data()));
    *componentHTML[component] << "<head><link rel=\"stylesheet\" type=\"text/css\" href=\"../../../style/style.css\"><title>35t: Run " << run << ", Subrun " << subrun <<"</title></head>" << std::endl << "<body><a href=\"http://lbne-dqm.fnal.gov\">" << std::endl << "  <div class=\"bannertop\"></div>" << std::endl << "</a></br>" << std::endl;;
    *componentHTML[component] << "<h1 align=center>" << component << "</h1>" << std::endl;
    *componentHTML[component] << "<center>Run " << run << ", Subrun " << subrun << " started " << fRunStartTime << "; monitoring last updated " << writeOutTime <<  "</br>" << "Events processed: " << eventsProcessed << "</center></br>" << std::endl;
  }

  fDataFile->cd();

  // Write the tree
  if (fDetailedMonitoring) fDataTree->Write();

  // Contruct MultiGraphs for the timing sync before saving
  //ConstructTimingSyncGraphs();

  // Save all the histograms as images and write to file
  for (int histIt = 0; histIt < fHistArray.GetEntriesFast(); ++histIt) {
    fCanvas->cd();
    fCanvas->Clear();
    TH1* _h = (TH1*)fHistArray.At(histIt);
    TObjArray* histTitle = TString(_h->GetTitle()).Tokenize(PathDelimiter);
    TObjArray* histName = TString(_h->GetName()).Tokenize(PathDelimiter);
    TLegend* _l = new TLegend(0.7,0.9,0.7,0.9,"","NDC");
    if (std::string(_h->GetName()).find(std::string("DNoise")) != std::string::npos) {
      _h->SetLineColor(8);
      TH1* _hRMS = (TH1*)fHistArray.FindObject(TString("RCE_")+TString(histName->At(1)->GetName())+TString("_ADC_RMS_Channel_All"));
      _hRMS->Draw("hist");
      _h->Draw((TString(histTitle->At(1)->GetName())+TString(" same")).Data());
      _l->AddEntry(_h->Clone(),"DNoise","l");
      _l->AddEntry(_hRMS->Clone(),"RMS","l");
    }
    else _h->Draw((char*)histTitle->At(1)->GetName());
    fCanvas->cd();
    _l->DrawClone();
    TPaveText *canvTitle = new TPaveText(0.05,0.92,0.6,0.98,"NDC");
    canvTitle->AddText((std::string(histTitle->At(0)->GetName())+": Run "+std::to_string(run)+", SubRun "+std::to_string(subrun)).c_str());
    canvTitle->SetBorderSize(1);
    canvTitle->Draw();
    if (fFigureLegends[_h->GetName()]) {
      fCanvas->SetRightMargin(0.2);
      fFigureLegends[_h->GetName()]->Draw();
    }
    else fCanvas->SetRightMargin(0.1);
    if (strstr(histTitle->At(2)->GetName(),"logy")) fCanvas->SetLogy(1);
    else fCanvas->SetLogy(0);
    if (strstr(histTitle->At(2)->GetName(),"logz")) fCanvas->SetLogz(1);
    else fCanvas->SetLogz(0);
    fCanvas->Modified();
    fCanvas->Update();
    TLine line = TLine();
    line.SetLineColor(2);
    line.SetLineWidth(1);
    line.SetLineStyle(7);
    TText text = TText();
    text.SetTextColor(2);
    double lowerY, upperY;
    if (strstr(histTitle->At(2)->GetName(),"logy")) {
      lowerY = TMath::Power(10,fCanvas->GetFrame()->GetY1());
      upperY = TMath::Power(10,fCanvas->GetFrame()->GetY2());
    }
    else {
      lowerY = fCanvas->GetFrame()->GetY1();
      upperY = fCanvas->GetFrame()->GetY2();
    }
    if (strstr(histName->At(0)->GetName(),"SSP"))
      for (unsigned int sspchan = 0; sspchan <= NSSPChannels; sspchan+=12) {
	line.DrawLine(sspchan,lowerY,sspchan,upperY);
	text.DrawText(sspchan+2,lowerY+0.001,(std::to_string(((int)sspchan/12)+1)).c_str());
      }
    else if (strstr(histName->At(0)->GetName(),"RCE"))// and strstr(histName->At(4)->GetName(),"Channel"))
      for (unsigned int rcechan = 0; rcechan <= NRCEChannels; rcechan+=128) {
	line.DrawLine(rcechan,lowerY,rcechan,upperY);
	text.DrawText(rcechan+5,lowerY+0.001,(std::to_string((int)rcechan/128)).c_str());
      }
    fCanvas->Update();
    fCanvas->SaveAs(fHistSaveDirectory+TString(histName->At(0)->GetName())+TString("/")+TString(_h->GetName())+imageType);
    fDataFile->cd();
    fDataFile->cd(histName->At(0)->GetName());
    _h->Write();
    *componentHTML[histName->At(0)->GetName()] << "<figure><a href=\"" << (TString(_h->GetName())+imageType).Data() << "\"><img src=\"" << (TString(_h->GetName())+imageType).Data() << "\" width=\"650\"></a><figcaption>" << fFigureCaptions.at(_h->GetName()) << "</figcaption></figure>" << std::endl;
    delete _l;
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
  if (fDetailedMonitoring) {
    fDataFile->cd();
    fDataFile->cd("RCE");
    for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
      hADCChannelMap.at(channel)->Write();
  }

  // Add run file
  ofstream tmp((fHistSaveDirectory+TString("run").Data()));
  tmp << run << " " << subrun;
  tmp.flush();
  tmp.close();

  mf::LogInfo("Monitoring") << "Monitoring for run " << run << ", subRun " << subrun << " has been updated and is viewable at http://lbne-dqm.fnal.gov/OnlineMonitoring/Run" << run << "Subrun" << subrun;

}

void OnlineMonitoring::MonitoringData::MakeHistograms() {

  /// Makes all histograms and adds them to an array for easier handling

  // Define all histograms
  // Nomenclature: Name is used to save the histogram, Title has format : histogramTitle_histDrawOptions_canvasOptions;x-title;y-title;z-title

  // General
  hNumSubDetectorsPresent = new TH1I("General__NumberOfSubdetectors_Total__All","Number of Subdetectors_\"colz\"_logy;Number of Subdetectors;",25,0,25);
  hNumSubDetectorsPresent->GetXaxis()->SetNdivisions(25);
  hNumSubDetectorsPresent->GetXaxis()->CenterLabels();
  fFigureCaptions["General__NumberOfSubdetectors_Total__All"] = "Number of subdetectors present in each event in the data (one entry per event)";
  hSubDetectorsWithData = new TH1I("General__SubdetectorsWithData_Present__All","Subdetectors With Data_\"colz\"_none;Subdetectors With Data;",24,0,24);
  hSubDetectorsWithData->GetXaxis()->SetLabelSize(0.025);
  fFigureCaptions["General__SubdetectorsWithData_Present__All"] = "Subdetectors with data in run";
  hSubDetectorsPresent = new TH1I("General__SubdetectorsPresent_Total__All","Subdetectors With Data (per event)_\"colz\"_logy;Subdetectors With Data;",24,0,24);
  hSubDetectorsPresent->GetXaxis()->SetLabelSize(0.025);
  fFigureCaptions["General__SubdetectorsPresent_Total__All"] = "Subdetectors with data per event";
  for (std::vector<std::string>::const_iterator compIt = DAQComponents.begin(); compIt != DAQComponents.end(); ++compIt) {
    hSubDetectorsWithData->GetXaxis()->SetBinLabel(std::distance(DAQComponents.begin(),compIt)+1, (*compIt).c_str());
    hSubDetectorsPresent->GetXaxis()->SetBinLabel(std::distance(DAQComponents.begin(),compIt)+1, (*compIt).c_str());
  }
  hSizeOfFiles = new TH1I("General__Last20Files_Size_RunSubrun_All","Data File Sizes_\"colz\"_none;Run&Subrun;Size (MB);",20,0,20);
  fFigureCaptions["General__Last20Files_Size_RunSubrun_All"] = "Size of the data files made by the DAQ for the last 20 runs";
  hSizeOfFilesPerEvent = new TH1D("General__Last20Files_SizePerEvent_RunSubrun_All","Size of Event in Data Files_\"colz\"_none;Run&Subrun;Size (MB/event);",20,0,20);
  fFigureCaptions["General__Last20Files_SizePerEvent_RunSubrun_All"] = "Size of event in each of the last 20 data files made by the DAQ (size of file / number of events in file)";
  // // SSP time sync
  // hSSPTimeSync = new TMultiGraph("General_SSP_TimeSync_Difference_MinChannelTime_All",
  // 				 "Max -- Min Average Trigger Times (All SSPs)_\"AL\"_none;Min average channel time;(Max-min) channel time");
  // fFigureCaptions[hSSPTimeSync->GetName()] = "Max-min average trigger times for all SSPs";
  // hSSPTimeSyncAverage = new TMultiGraph("General_SSP_TimeSync_Average_AverageChannelTime_All",
  // 					"Average SSP Trigger Time -- Average Channel Trigger Time_\"AL\"_none;Average channel trigger time;SSP average trigger time - average channel trigger time");
  // fFigureCaptions[hSSPTimeSyncAverage->GetName()] = "Average SSP trigger time - average channel trigger time for all SSPs";
  // for (unsigned int ssp = 0; ssp < NSSPs; ++ssp) {
  //   hTimeSyncSSPs[ssp] = new TGraph();
  //   hTimeSyncSSPs[ssp]->SetName(("General_SSP"+std::to_string(ssp)+"_TimeSync_Difference_MinChannelTime_All").c_str());
  //   hTimeSyncSSPs[ssp]->SetTitle("Max -- Min Average Trigger Times for SSP "+ssp);
  //   hTimeSyncSSPs[ssp]->SetLineColor(ssp+1);
  //   hTimeSyncSSPs[ssp]->SetMarkerColor(ssp+1);
  //   hTimeSyncSSPs[ssp]->SetLineStyle(ssp+2);
  //   hTimeSyncSSPs[ssp]->SetLineWidth(2);
  //   hTimeSyncAverageSSPs[ssp] = new TGraph();
  //   hTimeSyncAverageSSPs[ssp]->SetName(("General_SSP"+std::to_string(ssp)+"_TimeSync_Average_MinChannelTime_All").c_str());
  //   hTimeSyncAverageSSPs[ssp]->SetTitle("Max -- Min Average Trigger Times for All SSPs");
  //   hTimeSyncAverageSSPs[ssp]->SetLineColor(ssp+1);
  //   hTimeSyncAverageSSPs[ssp]->SetMarkerColor(ssp+1);
  //   hTimeSyncAverageSSPs[ssp]->SetLineStyle(ssp+2);
  //   hTimeSyncAverageSSPs[ssp]->SetLineWidth(2);
  // }

  // RCE hists
  hADCMeanChannelAPA1 = new TProfile("RCE_APA0_ADC_Mean_Channel_All","RCE ADC Mean APA0_\"histl\"_none;Channel;RCE ADC Mean",512,0,511);
  fFigureCaptions["RCE_APA0_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCMeanChannelAPA2 = new TProfile("RCE_APA1_ADC_Mean_Channel_All","RCE ADC Mean APA1_\"histl\"_none;Channel;RCE ADC Mean",512,512,1023);
  fFigureCaptions["RCE_APA1_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCMeanChannelAPA3 = new TProfile("RCE_APA2_ADC_Mean_Channel_All","RCE ADC Mean APA2_\"histl\"_none;Channel;RCE ADC Mean",512,1024,1535);
  fFigureCaptions["RCE_APA2_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCMeanChannelAPA4 = new TProfile("RCE_APA3_ADC_Mean_Channel_All","RCE ADC Mean APA3_\"histl\"_none;Channel;RCE ADC Mean",512,1536,2047);
  fFigureCaptions["RCE_APA3_ADC_Mean_Channel_All"] = "Mean ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCRMSChannelAPA1 = new TProfile("RCE_APA0_ADC_RMS_Channel_All","RCE ADC RMS APA0_\"histl\"_logy;Channel;RCE ADC RMS",512,0,511);
  fFigureCaptions["RCE_APA0_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCRMSChannelAPA2 = new TProfile("RCE_APA1_ADC_RMS_Channel_All","RCE ADC RMS APA1_\"histl\"_logy;Channel;RCE ADC RMS",512,512,1023);
  fFigureCaptions["RCE_APA1_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCRMSChannelAPA3 = new TProfile("RCE_APA2_ADC_RMS_Channel_All","RCE ADC RMS APA2_\"histl\"_logy;Channel;RCE ADC RMS",512,1024,1535);
  fFigureCaptions["RCE_APA2_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCRMSChannelAPA4 = new TProfile("RCE_APA3_ADC_RMS_Channel_All","RCE ADC RMS APA3_\"histl\"_logy;Channel;RCE ADC RMS",512,1536,2047);
  fFigureCaptions["RCE_APA3_ADC_RMS_Channel_All"] = "RMS of the ADC values for each channel read out by the RCEs (profiled over all events read)";
  hADCDNoiseRMSChannelAPA1 = new TProfile("RCE_APA0_ADC_DNoise_Channel_All","RCE DNoise APA0_\"hist\"_logy;Channel;DNoise/sqrt(2)",512,0,511);
  fFigureCaptions["RCE_APA0_ADC_DNoise_Channel_All"] = "DNoise (RMS(ADC[channel_i][tick_i]-ADC[channel_i+1][tick_i])/sqrt(2)) (green) plotted over RMS (blue) for APA0 (profiled over all events read)";
  hADCDNoiseRMSChannelAPA2 = new TProfile("RCE_APA1_ADC_DNoise_Channel_All","RCE DNoise APA1_\"hist\"_logy;Channel;DNoise/sqrt(2)",512,512,1023);
  fFigureCaptions["RCE_APA1_ADC_DNoise_Channel_All"] = "DNoise (RMS(ADC[channel_i][tick_i]-ADC[channel_i+1][tick_i])/sqrt(2)) (green) plotted over RMS (blue) for APA1 (profiled over all events read)";
  hADCDNoiseRMSChannelAPA3 = new TProfile("RCE_APA2_ADC_DNoise_Channel_All","RCE DNoise APA2_\"hist\"_logy;Channel;DNoise/sqrt(2)",512,1024,1535);
  fFigureCaptions["RCE_APA2_ADC_DNoise_Channel_All"] = "DNoise (RMS(ADC[channel_i][tick_i]-ADC[channel_i+1][tick_i])/sqrt(2)) (green) plotted over RMS (blue) for APA2 (profiled over all events read)";
  hADCDNoiseRMSChannelAPA4 = new TProfile("RCE_APA3_ADC_DNoise_Channel_All","RCE DNoise APA3_\"hist\"_logy;Channel;DNoise/sqrt(2)",512,1536,2047);
  fFigureCaptions["RCE_APA3_ADC_DNoise_Channel_All"] = "DNoise (RMS(ADC[channel_i][tick_i]-ADC[channel_i+1][tick_i])/sqrt(2)) (green) plotted over RMS (blue) for APA3 (profiled over all events read)";
  hFFTChannelRCE00 = new TProfile2D("RCE_RCE00_ADC_FFT_Channel_FirstEvent","ADC FFT for RCE00_\"colz\"_logz;Channel;FFT (MHz)",128,0,128,100,0,1);
  fFigureCaptions["RCE_RCE00_ADC_FFT_Channel_FirstEvent"] = "FFT of ADC values in RCE00";
  hFFTChannelRCE00->GetZaxis()->SetRangeUser(0,5000);
  hADCChannel = new TH2D("RCE__ADC_Value_Channel_All","ADC vs Channel_\"colz\"_logz;Channel;ADC Value",NRCEChannels,0,NRCEChannels,5000,0,5000);
  fFigureCaptions["RCE__ADC_Value_Channel_All"] = "ADC value for each channel (one entry per tick)";
  hTickRatioChannel = new TH2D("RCE__NumberOfTicks_Ratio_Channel_All","Ratio of Number of Tick to Max In Event_\"colz\"_none;Channel;Number of Ticks/Max Number of Ticks in Event",NRCEChannels,0,NRCEChannels,101,0,1.01);
  fFigureCaptions["RCE__NumberOfTicks_Ratio_Channel_All"] = "Number of ticks in a particular channel as ratio of maximum number of ticks across all channels in the event (filled once per channel per event) -- should be 1";
  hAvADCChannelEvent = new TH2D("RCE__ADC_Mean_Event_First100","Average RCE ADC Value_\"colz\"_none;Event;Channel",100,0,100,NRCEChannels,0,NRCEChannels);
  fFigureCaptions["RCE__ADC_Mean_Event_First100"] = "Average RCE ADC across a channel for an event, shown for the first 100 events";
  hTotalADCEvent = new TH1I("RCE__ADC_Total__All","Total RCE ADC_\"colz\"_logy;Total ADC;",100,0,1000000000);
  fFigureCaptions["RCE__ADC_Total__All"] = "Total RCE ADC recorded for an event across all channels (one entry per event)";
  hTotalRCEHitsEvent = new TH1I("RCE__Hits_Total__All","Total RCE Hits in Event_\"colz\"_logy;Total RCE Hits;",100,0,10000000);
  fFigureCaptions["RCE__Hits_Total__All"] = "Total number of hits recorded on the RCEs per event (one entry per event)";
  hTotalRCEHitsChannel = new TH1I("RCE__Hits_Total_Channel_All","Total RCE Hits by Channel_\"colz\"_none;Channel;Total RCE Hits;",NRCEChannels,0,NRCEChannels);
  fFigureCaptions["RCE__Hits_Total_Channel_All"] = "Total number of RCE hits in each channel across all events";
  hNumMicroslicesInMillislice = new TH1I("RCE__Microslices_Total_Millislice_All","Number of Microslices in Millislice_\"colz\"_none;Millislice;Number of Microslices;",16,100,116);
  hNumNanoslicesInMicroslice = new TH1I("RCE__Nanoslices_Total_Microslice_All","Number of Nanoslices in Microslice_\"colz\"_logy;Number of Nanoslices;",1000,0,1100);
  hNumNanoslicesInMillislice = new TH1I("RCE__Nanoslices_Total_Millislice_All","Number of Nanoslices in Millislice_\"colz\"_logy;Number of Nanoslices;",1000,0,11000);
  hTimesADCGoesOverThreshold = new TH1I("RCE__ADC_TimesOverThreshold__All","Times RCE ADC Over Threshold_\"colz\"_none;Times ADC Goes Over Threshold;",100,0,1000);
  fFigureCaptions["RCE__ADC_TimesOverThreshold__All"] = "Number of times an RCE hit goes over a set ADC threshold";
  hAsymmetry = new TProfile("RCE__ADC_Asymmetry__All","Asymmetry of Bipolar Pulse_\"colz\"_none;Channel;Asymmetry",NRCEChannels,0,NRCEChannels);
  fFigureCaptions["RCE__ADC_Asymmetry__All"] = "Asymmetry of the bipolar pulse measured on the induction planes, by channel (profiled across all events). Zero means completely symmetric pulse";
  hRCEBitCheckAnd = new TH2I("RCE__ADCBit_On_Channel_All","RCE ADC Bits Always On_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  fFigureCaptions["RCE__ADCBit_On_Channel_All"] = "Check for stuck RCE bits: bits which are always on";
  hRCEBitCheckOr = new TH2I("RCE__ADCBit_Off_Channel_All","RCE ADC Bits Always Off_\"colz\"_none;Channel;Bit",NRCEChannels,0,NRCEChannels,16,0,16);
  fFigureCaptions["RCE__ADCBit_Off_Channel_All"] = "Check for stuck RCE bits: bits which are always off";
  hLastSixBitsCheckOn = new TProfile("RCE__ADCLast6Bits_On_Channel_All","Last Six RCE ADC Bits On_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  fFigureCaptions["RCE__ADCLast6Bits_On_Channel_All"] = "Fraction of all RCE ADC values with the last six bits stuck on (profiled; one entry per ADC)";
  hLastSixBitsCheckOff = new TProfile("RCE__ADCLast6Bits_Off_Channel_All","Last Six RCE ADC Bits Off_\"colz\"_none;Channel;Fraction of ADCs with stuck bits",NRCEChannels,0,NRCEChannels);
  fFigureCaptions["RCE__ADCLast6Bits_Off_Channel_All"] = "Fraction of all RCE ADC values with the last six bits stuck off (profiled; one entry per ADC)";

  // SSP hists
  hWaveformMean = new TProfile("SSP__ADC_Mean_Channel_All","SSP ADC Mean_\"hist\"_none;Channel;Average Waveform",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_Mean_Channel_All"] = "Average waveform across SSP channels (profiled over all events)";
  hWaveformRMS = new TProfile("SSP__ADC_RMS_Channel_All","SSP ADC RMS_\"hist\"_none;Channel;RMS of Waveform",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_RMS_Channel_All"] = "RMS of the SSP waveforms (profiled across all events)";
  hWaveformPeakHeight = new TProfile("SSP__ADC_PeakHeight_Channel_All","Waveform Peak Height_\"hist\"_none;Channel;Peak Height",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_PeakHeight_Channel_All"] = "Peak height of the SSP waveforms (profiled across all events)";
  hWaveformIntegral = new TProfile("SSP__ADC_Integral_Channel_All","Waveform Integral_\"hist\"_none;Channel;Integral",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_Integral_Channel_All"] = "Integral of the SSP waveforms (profiled across all events)";
  hWaveformIntegralNorm = new TProfile("SSP__ADC_IntegralNorm_Channel_All","Waveform Integral (normalised by window size)_\"hist\"_none;Channel;Integral",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_IntegralNorm_Channel_All"] = "Normalised integral of the SSP waveforms (profiled across all events)";
  hWaveformPedestal = new TProfile("SSP__ADC_Pedestal_Channel_All","Waveform Pedestal_\"hist\"_none;Channel;Pedestal",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__ADC_Pedestal_Channel_All"] = "Pedestal of the SSP waveforms (profiled across all events)";
  hWaveformNumTicks = new TProfile("SSP__Ticks__Channel_All","Num Ticks in Trigger_\"hist\"_none;Channel;Number of Ticks",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__Ticks__Channel_All"] = "Number of ticks in each trigger";
  hSSPTriggerRate = new TProfile("SSP__Triggers_Rate_Channel_All","SSP Trigger Rate_\"hist\"_none;Channel;Trigger rate",NSSPChannels,0,NSSPChannels);
  fFigureCaptions[hSSPTriggerRate->GetName()] = "Trigger rate (defined as number of triggers / number of fragments) per channel";
  hNumberOfTriggers = new TH1I("SSP__Triggers_Total_Channel_All","Number of Triggers_\"hist\"_none;Channel;Number of Triggers",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__Triggers_Total_Channel_All"] = "Total number of triggers per channel";
  hTriggerFraction = new TProfile("SSP__Triggers_Fraction_Channel_All","Fraction of Events With Trigger_\"hist\"_none;Channel;Number of Triggers",NSSPChannels,0,NSSPChannels);
  fFigureCaptions["SSP__Triggers_Fraction_Channel_All"] = "Fraction of events with a trigger for each channel";

  // PTB hists
  hPTBTriggerRates = new TProfile("PTB__Trigger_HitRate_TriggerType_All","Trigger Rates_\"\"_none;Trigger;Hit rate (Hz)",9,1,10);
  hPTBTriggerRates->GetXaxis()->SetBinLabel(1,"Muon TSU EL-WU");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(2,"Muon TSU SU-NL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(3,"Muon TSU SL-NU");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(4,"Muon BSU RM-CL");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(5,"SSP");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(6,"Calibration C1");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(7,"Calibration C2");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(8,"Calibration C3");
  hPTBTriggerRates->GetXaxis()->SetBinLabel(9,"Calibration C4");
  fFigureCaptions[hPTBTriggerRates->GetName()] = "Average hit rates per millislice of the muon trigger system";

#ifdef OLD_CODE
  hPTBBlockLength = new TH1I("PTB__Payloads_Total_NumberPayloads_All","Total Number of Payloads_\"hist\"_none;Number of Payloads",400,0,400);
#else
  hPTBBlockLength = new TH1I("PTB__Payloads_Total_NumberPayloads_All","Total Number of Payloads_\"hist\"_none;Number of Payloads",1000,0,1000);
#endif
  fFigureCaptions[hPTBBlockLength->GetName()] = "Number of payloads in each event (filled once per event)";

  hPTBPayloadType = new TH1I("PTB__Payloads_Type_PayloadType_All","Payload Type_\"hist\"_logy;Payload Type",6,1,7);
  hPTBPayloadType->GetXaxis()->SetBinLabel(1,"Counter");
  hPTBPayloadType->GetXaxis()->SetBinLabel(2,"Trigger");
  hPTBPayloadType->GetXaxis()->SetBinLabel(3,"Calibration");
  hPTBPayloadType->GetXaxis()->SetBinLabel(4,"Timestamp");
  hPTBPayloadType->GetXaxis()->SetBinLabel(5,"Warning");
  hPTBPayloadType->GetXaxis()->SetBinLabel(6,"Other");
  fFigureCaptions[hPTBPayloadType->GetName()] = "Payload type (filled once per payload) -- other should be empty";

  hPTBTSUCounterHitRateWU = new TProfile("PTB_TSUWU_Hits_Mean_Counter_All","TSU WU Counter Hit Rate_\"\"_none;Counter number;Hit Rate (Hz)",10,1,11);
  hPTBTSUCounterHitRateWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateWU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUWU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU West Up counters";
  hPTBTSUCounterActivationTimeWU = new TProfile("PTB_TSUWU_ActivationTime_Mean_Counter_All","TSU WU Average Activation Time_\"\"_none;Counter number;Time",10,1,11);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeWU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUWU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU West Up counters";

  hPTBTSUCounterHitRateEL = new TProfile("PTB_TSUEL_Hits_Mean_Counter_All","TSU EL Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",10,1,11);
  hPTBTSUCounterHitRateEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterHitRateEL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUEL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU East Low counters";
  hPTBTSUCounterActivationTimeEL = new TProfile("PTB_TSUEL_ActivationTime_Mean_Counter_All","TSU EL Average Activation Time_\"\"_none;Counter number;Time",10,1,11);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->SetNdivisions(10);
  hPTBTSUCounterActivationTimeEL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUEL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU East Low counters";

  hPTBTSUCounterHitRateExtra = new TProfile("PTB_TSUExtra_Hits_Mean_Counter_All","TSU Empty Counter Bits Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",4,1,5);
  hPTBTSUCounterHitRateExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterHitRateExtra->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUExtra_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU extra counter bits (SHOULD BE EMPTY)";
  hPTBTSUCounterActivationTimeExtra = new TProfile("PTB_TSUExtra_ActivationTime_Mean_Counter_All","TSU Empty Counter Bits Average Activation Time_\"\"_none;Counter number;Time",4,1,5);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->SetNdivisions(4);
  hPTBTSUCounterActivationTimeExtra->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUExtra_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU extra counter bits (SHOULD BE EMPTY)";

  hPTBTSUCounterHitRateNU = new TProfile("PTB_TSUNU_Hits_Mean_Counter_All","TSU NU Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",6,1,7);
  hPTBTSUCounterHitRateNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUNU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Up counters";
  hPTBTSUCounterActivationTimeNU = new TProfile("PTB_TSUNU_ActivationTime_Mean_Counter_All","TSU NU Average Activation Time_\"\"_none;Counter number;Time",6,1,7);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUNU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Up counters";

  hPTBTSUCounterHitRateSL = new TProfile("PTB_TSUSL_Hits_Mean_Counter_All","TSU SL Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",6,1,7);
  hPTBTSUCounterHitRateSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUSL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU South Low counters";
  hPTBTSUCounterActivationTimeSL = new TProfile("PTB_TSUSL_ActivationTime_Mean_Counter_All","TSU SL Average Activation Time_\"\"_none;Counter number;Time",6,1,7);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUSL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU South Low counters";

  hPTBTSUCounterHitRateNL = new TProfile("PTB_TSUNL_Hits_Mean_Counter_All","TSU NL Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",6,1,7);
  hPTBTSUCounterHitRateNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateNL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUNL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Low counters";
  hPTBTSUCounterActivationTimeNL = new TProfile("PTB_TSUNL_ActivationTime_Mean_Counter_All","TSU NL Average Activation Time_\"\"_none;Counter number;Time",6,1,7);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeNL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUNL_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Low counters";

  hPTBTSUCounterHitRateSU = new TProfile("PTB_TSUSU_Hits_Mean_Counter_All","TSU SU Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",6,1,7);
  hPTBTSUCounterHitRateSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterHitRateSU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUSU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the TSU North Low counters";
  hPTBTSUCounterActivationTimeSU = new TProfile("PTB_TSUSU_ActivationTime_Mean_Counter_All","TSU SU Average Activation Time_\"\"_none;Counter number;Time",6,1,7);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->SetNdivisions(6);
  hPTBTSUCounterActivationTimeSU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_TSUSU_ActivationTime_Mean_Counter_All"] = "Average activation time for the TSU North Low counters";

  hPTBBSUCounterHitRateRM = new TProfile("PTB_BSURM_Hits_Mean_Counter_All","BSU RM Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",16,1,17);
  hPTBBSUCounterHitRateRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterHitRateRM->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSURM_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU RM counters";
  hPTBBSUCounterActivationTimeRM = new TProfile("PTB_BSURM_ActivationTime_Mean_Counter_All","BSU RM Average Activation Time_\"\"_none;Counter number;Time",16,1,17);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->SetNdivisions(16);
  hPTBBSUCounterActivationTimeRM->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSURM_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU RM counters";

  hPTBBSUCounterHitRateCU = new TProfile("PTB_BSUCU_Hits_Mean_Counter_All","BSU CU Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",10,1,11);
  hPTBBSUCounterHitRateCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateCU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSUCU_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU CU counters";
  hPTBBSUCounterActivationTimeCU = new TProfile("PTB_BSUCU_ActivationTime_Mean_Counter_All","BSU CU Average Activation Time_\"\"_none;Counter number;Time",10,1,11);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeCU->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSUCU_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU CU counters";

  hPTBBSUCounterHitRateCL = new TProfile("PTB_BSUCL_Hits_Mean_Counter_All","BSU CL Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",13,1,14);
  hPTBBSUCounterHitRateCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterHitRateCL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSUCL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU CL counters";
  hPTBBSUCounterActivationTimeCL = new TProfile("PTB_BSUCL_ActivationTime_Mean_Counter_All","BSU CL Average Activation Time_\"\"_none;Counter number;Time",13,1,14);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->SetNdivisions(13);
  hPTBBSUCounterActivationTimeCL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSUCL_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU CL counters";

  hPTBBSUCounterHitRateRL = new TProfile("PTB_BSURL_Hits_Mean_Counter_All","BSU RL Counter Hit Rate_\"\"_none;Counter number;Hit rate (Hz)",10,1,11);
  hPTBBSUCounterHitRateRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterHitRateRL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSURL_Hits_Mean_Counter_All"] = "Average hit rate in a millislice for the BSU RL counters";
  hPTBBSUCounterActivationTimeRL = new TProfile("PTB_BSURL_ActivationTime_Mean_Counter_All","BSU RL Average Activation Time_\"\"_none;Counter number;Time",10,1,11);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->SetNdivisions(10);
  hPTBBSUCounterActivationTimeRL->GetXaxis()->CenterLabels();
  fFigureCaptions["PTB_BSURL_ActivationTime_Mean_Counter_All"] = "Average activation time for the BSU RL counters";

  // The order the histograms are added will be the order they're displayed on the web!
  fHistArray.Add(hNumSubDetectorsPresent); 
  fHistArray.Add(hSubDetectorsPresent); fHistArray.Add(hSubDetectorsWithData);
  fHistArray.Add(hSizeOfFiles); fHistArray.Add(hSizeOfFilesPerEvent);

  //fHistArray.Add(hSSPTimeSync); fHistArray.Add(hSSPTimeSyncAverage);

  fHistArray.Add(hADCMeanChannelAPA1); fHistArray.Add(hADCMeanChannelAPA2);
  fHistArray.Add(hADCMeanChannelAPA3); fHistArray.Add(hADCMeanChannelAPA4);
  fHistArray.Add(hADCRMSChannelAPA1); fHistArray.Add(hADCRMSChannelAPA2);
  fHistArray.Add(hADCRMSChannelAPA3); fHistArray.Add(hADCRMSChannelAPA4);
  fHistArray.Add(hADCDNoiseRMSChannelAPA1); fHistArray.Add(hADCDNoiseRMSChannelAPA2);
  fHistArray.Add(hADCDNoiseRMSChannelAPA3); fHistArray.Add(hADCDNoiseRMSChannelAPA4);
  fHistArray.Add(hADCChannel); fHistArray.Add(hFFTChannelRCE00);
  fHistArray.Add(hTickRatioChannel);
  fHistArray.Add(hAvADCChannelEvent); fHistArray.Add(hTotalRCEHitsChannel);
  fHistArray.Add(hTotalADCEvent); fHistArray.Add(hTotalRCEHitsEvent);
  fHistArray.Add(hAsymmetry); fHistArray.Add(hTimesADCGoesOverThreshold);
  fHistArray.Add(hRCEBitCheckAnd); fHistArray.Add(hRCEBitCheckOr);
  fHistArray.Add(hLastSixBitsCheckOn); fHistArray.Add(hLastSixBitsCheckOff);
  //fHistArray.Add(hNumMicroslicesInMillislice); 

  fHistArray.Add(hWaveformMean); fHistArray.Add(hWaveformRMS);
  fHistArray.Add(hWaveformPeakHeight); fHistArray.Add(hWaveformPedestal);
  fHistArray.Add(hWaveformIntegral); fHistArray.Add(hWaveformIntegralNorm);
  fHistArray.Add(hSSPTriggerRate);
  fHistArray.Add(hNumberOfTriggers); fHistArray.Add(hTriggerFraction);
  fHistArray.Add(hWaveformNumTicks);

  fHistArray.Add(hPTBTriggerRates);
  fHistArray.Add(hPTBBlockLength); fHistArray.Add(hPTBPayloadType);
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

}

void OnlineMonitoring::MonitoringData::MakeDetailedHistograms() {

  /// Constructs histograms and other data objects used when monitoring in detailed mode

  for (unsigned int chan = 0; chan < NRCEChannels; ++chan)
    hADCChannelMap[chan] = new TProfile("RCE_ADCChannel"+TString(std::to_string(chan)),"RCE ADC v Tick for Channel "+TString(std::to_string(chan))+";Tick;ADC;",5000,0,5000);
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice) {
    hAvADCMillislice[millislice] = new TH1D("RCE_AvADCMillislice"+TString(std::to_string(millislice)),"Av ADC for Millislice "+TString(std::to_string(millislice))+";Event;Av ADC;",10000,0,10000);
    hAvADCMillisliceChannel[millislice] = new TH1D("RCE_AvADCMillisliceChannel"+TString(std::to_string(millislice)),"Av ADC v Channel for Millislice "+TString(std::to_string(millislice))+";Channel;Av ADC;",128,0,128);
  }

}

void OnlineMonitoring::MonitoringData::MakeScopeHistograms() {

  /// Constructs histograms and other data objects used when running the DAQ in scope mode

  hScopeTrace1s = new TProfile("RCE__ScopeTrace__Time_All","RCE Scope Trace_\"hist\"_none;Time (s);ADC",100,0,1);
  fFigureCaptions[hScopeTrace1s->GetName()] = "Scope mode: trace over 1s of running";
  hScopeTraceFFT1s = new TH1F("RCE__ScopeTrace_FFT_Frequency_All","RCE FFT Scope Trace_\"hist\"_none;Freq (Hz);",100,0,1);
  fFigureCaptions[hScopeTraceFFT1s->GetName()] = "Scope mode: FFT of trace over 1s of running";

  fHistArray.Add(hScopeTrace1s); fHistArray.Add(hScopeTraceFFT1s);

}

// void OnlineMonitoring::MonitoringData::ConstructTimingSyncGraphs() {

//   TLegend* sspTimeSyncLegend = new TLegend(0.8,0.5,0.9,0.9);

//   bool should_draw = false;
//   for (unsigned int ssp = 0; ssp < NSSPs; ++ssp) {
//     if (!hTimeSyncSSPs[ssp]) continue;
//     if (hTimeSyncSSPs[ssp]->GetN() > 0) {
//       should_draw = true;
//       hSSPTimeSync->Add(hTimeSyncSSPs[ssp]);
//       sspTimeSyncLegend->AddEntry(hTimeSyncSSPs[ssp],("SSP "+std::to_string(ssp+1)).c_str(),"l");
//     }
//   }
//   if (should_draw)
//     fFigureLegends[hSSPTimeSync->GetName()] = sspTimeSyncLegend;

//   TLegend* sspTimeSyncAverageLegend = new TLegend(0.8,0.5,0.9,0.9);

//   should_draw = false;
//   for (unsigned int ssp = 0; ssp < NSSPs; ++ssp) {
//     if (!hTimeSyncAverageSSPs[ssp]) continue;
//     if (hTimeSyncAverageSSPs[ssp]->GetN() > 0) {
//       should_draw = true;
//       hSSPTimeSyncAverage->Add(hTimeSyncAverageSSPs[ssp]);
//       sspTimeSyncAverageLegend->AddEntry(hTimeSyncAverageSSPs[ssp],("SSP "+std::to_string(ssp+1)).c_str(),"l");
//     }
//   }
//   if (should_draw)
//     fFigureLegends[hSSPTimeSyncAverage->GetName()] = sspTimeSyncAverageLegend;

// }
