////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: MonitoringPedestal
// File: MonitoringPedestal.cxx
// Author:  Gabriel Santucci (gabriel.santucci@stonybrook.edu), Sept 2015 
//          based on 
//          Mike Wallbank's MonitoringData.cxx
//
// Class to handle the creation of monitoring Pedestal/Noise Runs.
// Owns all the data containers and has methods for filling with online data and for writing and
// saving the output in relevant places.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MonitoringPedestal.hxx"

void OnlineMonitoring::MonitoringPedestal::BeginMonitoring(int run, int subrun) {

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
  fDataFile = new TFile(HistSaveDirectory+TString("monitoringPedestalRun"+std::to_string(run)+"Subrun"+std::to_string(subrun)+".root"),"RECREATE");
  
  // Tree
  if (fMakeTree) {
    fDataTree = new TTree("RawData","Raw Data");
    fDataTree->Branch("RCE",&fRCEADC);
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
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel){
    hADCChannel[channel]                = new TProfile("ADCChannel"+TString(std::to_string(channel)),"RCE ADC v Tick for Channel "+TString(std::to_string(channel))+";Tick;ADC;",5000,0,5000);
    //Pedestal/Noise exclusive
    hADCUnfiltered[channel]            = new TH1F(Form("ADCUnfiltered_%d",channel),Form("ADC Distribution of Channel %d_\"histl\"_none;", channel), 4096, 0, 4096);
    hADCFiltered[channel]              = new TH1F(Form("ADCFiltered_%d",channel),Form("Filtered ADC Distribution of Channel %d_\"histl\"_none;",channel), 4096, 0, 4096);
  }
  hPedestal                            = new TH1F("Pedestal", "Pedestal Distribution_\"\"_none; Pedestal;",4096,0,4096);
  hNoise                               = new TH1F("Noise", "Noise Distribution_\"\"_none; Noise;",500, 0, 500);
  hMeanDiff                            = new TH1F("MeanDiff", "Difference of the Gaussian Mean 1st - 2nd Fits_\"\"_none;", 200, -1.,1.);
  hSigmaDiff                           = new TH1F("SigmaDiff", "Difference of the Gaussian Sigma 1st - 2nd Fits_\"\"_none;", 200, -1.,1.);
  hMeanFiltDiff                        = new TH1F("MeanFiltDiff", "Difference of the Gaussian Mean Filtered - Unfiltered Fits_\"\"_none;", 200, -1.,1.);
  hSigmaFiltDiff                       = new TH1F("SigmaFiltDiff", "Difference of the Gaussian Sigma Filtered - Unfiltered Fits_\"\"_none;", 200, -1.,1.);
  hPedestalChan                        = new TProfile("PedestalChannel","Pedestal per Channel_\"histl\"_none;Channel;Pedestal",NRCEChannels,0,NRCEChannels);
  hNoiseChan                           = new TProfile("NoiseChannel","Noise per Channel_\"histl\"_none;Channel;Pedestal",NRCEChannels,0,NRCEChannels);
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice) {
    hAvADCMillislice[millislice]        = new TH1D("AvADCMillislice"+TString(std::to_string(millislice)),"Av ADC for Millislice "+TString(std::to_string(millislice))+";Event;Av ADC;",10000,0,10000);
    hAvADCMillisliceChannel[millislice] = new TH1D("AvADCMillisliceChannel"+TString(std::to_string(millislice)),"Av ADC v Channel for Millislice "+TString(std::to_string(millislice))+";Channel;Av ADC;",128,0,128);
  }
  for (std::vector<int>::const_iterator debugchannel = DebugChannels.begin(); debugchannel != DebugChannels.end(); ++debugchannel)
    hDebugChannelHists[(*debugchannel)] = new TH1D("Channel"+TString(std::to_string(*debugchannel))+"SingleEvent","Channel "+TString(std::to_string(*debugchannel))+" for Single Event",5000,0,5000);

  // General
  hNumSubDetectorsPresent = new TH1I("NumSubDetectorsPresent","Number of Subdetectors_\"colz\"_logy;Number of Subdetectors;",25,0,25);
  hSizeOfFiles            = new TH1I("SizeOfFiles","Data File Sizes_\"colz\"_none;Run&Subrun;Size (bytes);",20,0,20);
  hSizeOfFilesPerEvent    = new TH1D("SizeOfFilesPerEvent","Size of Event in Data Files_\"colz\"_none;Run&Subrun;Size (bytes/event);",20,0,20);

  // Add all histograms to the array for saving
  AddHists();

}

void OnlineMonitoring::MonitoringPedestal::EndMonitoring() {

  /// Clear up after writing out the monitoring data

  // Free up all used memory
  // for (unsigned int interestingchannel = 0; interestingchannel < DebugChannels.size(); ++interestingchannel)
  //   hDebugChannelHists.at(DebugChannels.at(interestingchannel))->Delete();
  for (unsigned int millislice = 0; millislice < NRCEMillislices; ++millislice)
    hAvADCMillislice.at(millislice)->Delete();
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel)
    hADCChannel.at(channel)->Delete();
  fDataFile->Close();
  delete fDataFile;

  // Free the memory for the histograms
  fHistArray.Delete();

}

void OnlineMonitoring::MonitoringPedestal::FillTree(RCEFormatter const& rceformatter) {
  fRCEADC = rceformatter.ADCVector();
  fDataTree->Fill();
}

void OnlineMonitoring::MonitoringPedestal::GeneralMonitoring() {

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

void OnlineMonitoring::MonitoringPedestal::GetPedestal(std::vector<int> ADCs, int channel, double mean, double rms){

  std::vector<int> copy(ADCs);

  mygaus = new TF1("mygaus","gaus",mean-3*rms,mean+3*rms);

  for (unsigned int itick = 0; itick < ADCs.size(); ++itick){
    if (int(ADCs.at(itick)&0x3F) == 0 && int(ADCs.at(itick)&0x3F) == 63) continue;
    else hADCUnfiltered.at(channel)->Fill(ADCs.at(itick));
  }

  hADCUnfiltered.at(channel)->Fit(mygaus, "R");
  double meanG = mygaus->GetParameter(1);
  double sigmaG = mygaus->GetParameter(2);
  mygaus = new TF1("mygaus","gaus",meanG-3*sigmaG,meanG+3*sigmaG);
  hADCUnfiltered.at(channel)->Fit(mygaus, "R");
  double meanG2 = mygaus->GetParameter(1);
  double sigmaG2 = mygaus->GetParameter(2);

  hMeanDiff->Fill(meanG - meanG2);
  hSigmaDiff->Fill(sigmaG - sigmaG2);

  hPedestal->Fill(meanG2);
  hNoise->Fill(sigmaG2);
  hPedestalChan->Fill(channel, meanG2);
  hNoiseChan->Fill(channel, sigmaG2);

  for (unsigned int itick = 0; itick < copy.size(); ++itick){
    if ((int(copy.at(itick)&0x3F) == 0 && int(copy.at(itick)&0x3F) == 63) || (itick + 5 > copy.size() ) ) continue;
    else
      if( (copy.at(itick+1) > copy.at(itick) && copy.at(itick+2) > copy.at(itick+1) && copy.at(itick+3) > copy.at(itick+2) && copy.at(itick+4) > copy.at(itick+3) && copy.at(itick+5) > copy.at(itick+4) ) || ( copy.at(itick+1) < copy.at(itick) && copy.at(itick+2) < copy.at(itick+1) && copy.at(itick+3) < copy.at(itick+2) && copy.at(itick+4) < copy.at(itick+3) && copy.at(itick+5) < copy.at(itick+4) )){
        if(itick < 100)
          copy.erase(copy.begin(), copy.begin() + itick + 100);
        else if(itick + 100 > copy.size())
          copy.erase(copy.begin() + itick, copy.end());
        else
          copy.erase(copy.begin() + itick, copy.begin() + itick + 100);
      }
  }

  for (unsigned int itick = 0; itick < ADCs.size(); ++itick)
    hADCFiltered.at(channel)->Fill(ADCs.at(itick));

  hADCFiltered.at(channel)->Fit(mygaus, "R");
  double meanGF = mygaus->GetParameter(1);
  double sigmaGF = mygaus->GetParameter(2);

  hMeanFiltDiff->Fill(meanG2 - meanGF);
  hSigmaFiltDiff->Fill(sigmaG2 - sigmaGF);

}
void OnlineMonitoring::MonitoringPedestal::RCEMonitoring(RCEFormatter const& rceformatter) {

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

    GetPedestal(ADCs.at(channel), channel, mean, rms);

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
void OnlineMonitoring::MonitoringPedestal::WriteMonitoringPedestal(int run, int subrun) {
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
  for (unsigned int channel = 0; channel < NRCEChannels; ++channel){
    hADCChannel.at(channel)->Write();
    //Pedestal/Noise exclusive
    hADCFiltered.at(channel)->Write();
    hADCUnfiltered.at(channel)->Write();
  }
  hMeanDiff->Write();
  hSigmaDiff->Write();
  hMeanFiltDiff->Write();
  hSigmaFiltDiff->Write();

  // Add run file
  ofstream tmp((HistSaveDirectory+TString("run").Data()));
  tmp << run << " " << subrun;
  tmp.flush();
  tmp.close();
  system(("chmod -R a=rwx "+std::string(HistSaveDirectory)).c_str());  

  mf::LogInfo("Monitoring") << "Monitoring for run " << run << ", subRun " << subrun << " is viewable at http://lbne-dqm.fnal.gov/OnlineMonitoring/Run" << run << "Subrun" << subrun;

}

void OnlineMonitoring::MonitoringPedestal::StartEvent(int eventNumber, bool maketree) {
  fEventNumber = eventNumber;
  fMakeTree = maketree;
  this->Reset();
}

void OnlineMonitoring::MonitoringPedestal::Reset() {

  fRCEADC.clear();

  fTotalADC = 0;

  fTotalRCEHitsEvent = 0;

  fTimesADCGoesOverThreshold = 0;

  fNumSubDetectorsPresent = 0;

}

void OnlineMonitoring::MonitoringPedestal::AddHists() {

  /// Adds all the histograms to an array for easier handling

  //Pedestal/Noise exclusive                        
  fHistArray.Add(hPedestal); fHistArray.Add(hNoise); 
  fHistArray.Add(hPedestalChan); fHistArray.Add(hNoiseChan);

  // The order the histograms are added will be the order they're displayed on the web!
  fHistArray.Add(hADCMeanChannel); fHistArray.Add(hADCRMSChannel);
  fHistArray.Add(hAvADCAllMillislice); fHistArray.Add(hRCEDNoiseChannel);
  fHistArray.Add(hAvADCChannelEvent); fHistArray.Add(hTotalRCEHitsChannel);
  fHistArray.Add(hTotalADCEvent); fHistArray.Add(hTotalRCEHitsEvent);
  fHistArray.Add(hAsymmetry); fHistArray.Add(hTimesADCGoesOverThreshold);
  fHistArray.Add(hRCEBitCheckAnd); fHistArray.Add(hRCEBitCheckOr);
  fHistArray.Add(hLastSixBitsCheckOn); fHistArray.Add(hLastSixBitsCheckOff);
  fHistArray.Add(hNumMicroslicesInMillislice); 

  fHistArray.Add(hSizeOfFiles); fHistArray.Add(hSizeOfFilesPerEvent);
  fHistArray.Add(hNumSubDetectorsPresent);
  
  fFigureCaptions["Pedestal"] = "Pedestal Distribution";
  fFigureCaptions["Noise"] = "Noise Distribution";
  fFigureCaptions["PedestalChannel"] = "Pedestal per Channel";
  fFigureCaptions["NoiseChannel"] = "Noise per Channel";
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
  fFigureCaptions["SizeOfFiles"] = "Size of the data files made by the DAQ for the last 20 runs";
  fFigureCaptions["SizeOfFilesPerEvent"] = "Size of event in each of the last 20 data files made by the DAQ (size of file / number of events in file)";
  fFigureCaptions["NumSubDetectorsPresent"] = "Number of subdetectors present in each event in the data (one entry per event)";

}


