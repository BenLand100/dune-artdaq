////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: MonitoringPedestal
// File: MonitoringPedestal.cxx
// Author:  Gabriel Santucci (gabriel.santucci@stonybrook.edu), Dec 2015 
//          based on 
//          Mike Wallbank's MonitoringData.cxx
//
// Class to handle the creation of monitoring Pedestal/Noise Runs.
// Owns all the data containers and has methods for filling with online data and for writing and
// saving the output in relevant places.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MonitoringPedestal.hxx"

void PedestalMonitoring::MonitoringPedestal::BeginMonitoring(int run, int subrun) {

  // Get directory for this run                                                                      
  std::ostringstream directory;
  directory << HistSavePath << "Run" << run << "/";
  // Make the directories to save the files                                                          
  std::ostringstream cmd;
  cmd << "touch " << directory.str() << "; rm -rf " << directory.str() << "; mkdir " << directory.str();
  system(cmd.str().c_str());

  dumb_file.open(HistSavePath + "/dumb.txt", std::ios::out);
  dumb_file << run << std::endl;
  dumb_file.close();
  ondatabase_file.open(directory.str() + Form("/online_databaseRun_%d.csv",run),std::ios::out);
  offdatabase_file.open(directory.str() + Form("/offline_databaseRun_%d.csv",run),std::ios::out);
  nosignal_file.open(directory.str() + Form("/nosignalRun_%d.csv",run), std::ios::out);

  TString rootfile;
  rootfile = HistSavePath + "Run" + std::to_string(run)+ "/monitoringPedestal_Run" + std::to_string(run) + "Subrun" + std::to_string(subrun) + ".root";

  fDataFile = new TFile(rootfile,"RECREATE");

  for (int ichan = 0; ichan < NumChannels; ++ichan){
    hADC[ichan] = new TH1I(Form("hADC_%d", ichan),"",4096,-0.5,4095.5 );
    hADCTotal[ichan] = new TH1I(Form("hADCTotal_%d", ichan),"",4096,-0.5,4095.5 );
    hWaveform[ichan] = new TH1I(Form("hWaveform_%d", ichan),"",11000,0,11000*0.5 );
    hMeanPiece[ichan] = new TH1F(Form("hMeanPiece_%d", ichan), "", 1024,-0.5,4095.5);
    hRMSPiece[ichan] = new TH1F(Form("hRMSPiece_%d", ichan), "", 1024,-0.5,4095.5);
  }

  hDiffMean = new TH1F("hDiffMean", "",50,-50,50);
  hDiffRMS = new TH1F("hDiffRMS", "",20,-10,10);
  hMean = new TH1F("hMean", "",2050,0,4100);
  hRMS = new TH1F("hRMS", "",500,0,1000);
  hPed = new TH1F("hPed", "",2050,0,4100);
  hNoise = new TH1F("hNoise", "",500,0,1000);
  hDiffMeanChannel = new TH1F("hDiffMeanChannel","",2048,0.5,2048.5);
  hDiffRMSChannel = new TH1F("hDiffRMSChannel","",2048,0.5,2048.5);
  hMeanChannel = new TH1F("hMeanChannel","",2048,0.5,2048.5);
  hRMSChannel = new TH1F("hRMSChannel","",2048,0.5,2048.5);
  hPedChannel = new TH1F("hPedChannel","",2048,0.5,2048.5);
  hNoiseChannel = new TH1F("hNoiseChannel","",2048,0.5,2048.5);
  hDroppedMean = new TH1F("hDroppedMean","",2048,0.5,2048.5);
  hDroppedRMS = new TH1F("hDroppedRMS","",2048,0.5,2048.5);
  hSizes = new TH1I("hSizes","",2048,0.5,2048.5);
  hAllSizes = new TH1F("hAllSizes","",2048,0.5,2048.5);
  hRelSizes = new TH1F("hRelSizes","",2048,0.5,2048.5);
  hIssues = new TH1I("hIssues","",numissues,0.5,numissues+0.5);

}

void PedestalMonitoring::MonitoringPedestal::EndMonitoring() {
  fDataFile->Close();
  delete fDataFile;
  ondatabase_file.close();
  offdatabase_file.close();
  nosignal_file.close();
  log_file.close();
}

int PedestalMonitoring::MonitoringPedestal::RCEMonitoring(RCEFormatter const& rceformatter, int window){

  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();
  std::vector<int> nostuck;
  std::vector<int> offchannels;

  int stuckADC;
  int ADC = 0;
  int vec_size = 0;
  int fifty_percent = 0;
  int num_channels = 0;

  int issues[numissues]; for (int issue = 0; issue < numissues; ++issue) issues[issue] = 0;

  float average_stuck = 0.;

  bool channels_ok = false;

  // reading log file to get run number
  ifstream infile;
  infile.open(HistSavePath + "/dumb.txt",std::ios::in);
  std::string word;
  while (infile >> word)
    run = atoi(word.c_str());
  infile.close();

  ifstream off_file;
  off_file.open(HistSavePath + "/off_map.txt",std::ios::in);
  while(off_file >> word){
    offchannels.push_back(atoi(word.c_str()));
  }
  off_file.close();
  
  std::ostringstream directory;
  directory << HistSavePath << "Run" << run << "/";

  ofstream logfile;
  logfile.open(directory.str() + "/log.txt",std::ios::app);
  ofstream badlist;
  badlist.open(directory.str() + "/badchannels.txt",std::ios::out);

  //check if at least one channel has data.
  for (unsigned int ichannel = 0; ichannel < ADCs.size(); ichannel++){ 
    if (ADCs.at(ichannel).size()){
      channels_ok = true;
      if(ichannel%128==0)
	logfile << "channel " << ichannel << "/" << ADCs.size() << ", RCE " << int(ichannel/128) << std::endl;
    }
  }
    
  if(channels_ok){
    logfile << event << std::endl;
    std::cout << "Running pedestal analysis..." << std::endl;

    for (unsigned int ichannel = 0; ichannel < ADCs.size(); ichannel++) {
      ADC = 0;
      vec_size = 0;
      mean = 0;
      rms = 0;
      meantot = 0.;
      rmstot = 0;
      nostuck.clear();

      if (!ADCs.at(ichannel).size()){
	issues[0]++;
	nosignal_file << Form("%4d",ichannel) << ", " 
		      << 0 << ", " << 0
		      << ", " << 0  << ", " 
		      << 0 << ", " << run << ", 1" << std::endl;
	
	ondatabase_file << ichannel << ", 0, 0, 0, 0, " << run
			<< ", 1"<< std::endl;
	offdatabase_file << offchannels.at(ichannel) << ", 0, 0, 0, 0, " << run
	<< ", 1"<< std::endl;

	continue;
      }

      num_channels++;
      for(unsigned int itick=0;itick<ADCs.at(ichannel).size();itick++){
	ADC = ADCs.at(ichannel).at(itick);
	hADCTotal[ichannel]->Fill(ADC);
	hWaveform[ichannel]->SetBinContent(itick+1, ADC );
	meantot += ADC;
	rmstot += ADC*ADC;
	stuckADC = ADC & 0x3F;
	if(stuckADC==0 or stuckADC==63) continue;
	else {
	  nostuck.push_back(ADC);
	  hADC[ichannel]->Fill(ADC);
	  mean += ADC;
	  rms += ADC*ADC;
	  vec_size++;
	}
      }
      
      meantot /= ADCs.at(ichannel).size();
      rmstot /= ADCs.at(ichannel).size();
      rmstot = sqrt(rmstot - (meantot*meantot));

      if(vec_size == 0){
	mean = ADCs.at(ichannel).at(0);
	rms = 0;
	meanerr = 0.;
	rmserr = 0;
      }
      else{
	mean /= vec_size;
	rms /= vec_size;
	rms = sqrt( rms - (mean*mean) );
	meanerr = rms/(sqrt(vec_size));
	rmserr = rms/(sqrt(2*vec_size));
      }

      pathology = false;
      
      if((abs(meantot-mean) > 10 or abs(rmstot-rms) > 10) and mean < 1000){
        logfile << "Channel " << ichannel << " - Stuck ADCs make a difference." << std::endl;
	logfile << meantot << ", " << mean << ", " << rmstot << ", " << rms << std::endl;
	pathology = true;
	issues[2]++;
      }
      
      hMean->Fill(mean);
      hRMS->Fill(rms);
      hMeanChannel->SetBinContent(ichannel,mean);
      hRMSChannel->SetBinContent(ichannel,rms);

      hSizes->SetBinContent(ichannel, ADCs.at(ichannel).size());
      hAllSizes->SetBinContent(ichannel, float(nostuck.size())/float(ADCs.at(ichannel).size()));
      if (float(nostuck.size())/float(ADCs.at(ichannel).size()) > 0.5) 
	fifty_percent++;
      average_stuck += float(ADCs.at(ichannel).size() - nostuck.size())/float(ADCs.at(ichannel).size());


      //**************************************************************************************
      //Signal Fiding Algorithm
      // here we calculate pedestal/noise in the presence of signal in the data.

      std::vector<double> MeanPiece;
      std::vector<double> RMSPiece;
      
      if(int(nostuck.size())<window){
	if(nostuck.size() == 0){
	  issues[1]++;
	  logfile << "Channel " << ichannel
		  << ": All stuck ADCs" << std::endl;
	}
	else{
	  issues[3]++;
	  logfile << "Channel " << ichannel 
		  << ": Not enough data, size = " << nostuck.size() << std::endl;
	}
	pedestal_mean = ADCs.at(ichannel).at(0);
	noise_mean = 0;
	pedestalerr = 0;
	noiseerr = 0;
      }

      else{
	for (int ipiece=0; ipiece < int(nostuck.size()/window); ++ipiece){
	  if(window+window*ipiece < int(nostuck.size() )){
	    MeanPiece.push_back( TMath::Mean(nostuck.begin() + window*ipiece, nostuck.begin()+window+window*ipiece ) );
	    RMSPiece.push_back( TMath::RMS(nostuck.begin()+window*ipiece, nostuck.begin()+window+window*ipiece) );
	  }
	  else{
	    MeanPiece.push_back( TMath::Mean(nostuck.begin() + window*ipiece, nostuck.end()) );
	    RMSPiece.push_back( TMath::RMS(nostuck.begin()+window*ipiece, nostuck.end()) );
	  }
	}
	
	double MEAN_Mean = TMath::Mean(MeanPiece.begin(),MeanPiece.end());
	double RMS_Mean = TMath::RMS(MeanPiece.begin(),MeanPiece.end());
	double MEAN_RMS = TMath::Mean(RMSPiece.begin(),RMSPiece.end());
	double RMS_RMS = TMath::RMS(RMSPiece.begin(),RMSPiece.end());
	
	pedestal_mean = 0;
	pedestal_rms = 0;
	noise_mean = 0;
	noise_rms = 0;
	pedestalerr = 0;
	noiseerr = 0;
	
	int dropped = 0;
	for (unsigned int ipiece=0; ipiece < MeanPiece.size()-1; ++ipiece){
	  hMeanPiece[ichannel]->Fill(MeanPiece.at(ipiece));
	  if(abs(MeanPiece.at(ipiece) - MEAN_Mean) < 3*RMS_Mean) {
	    pedestal_mean += MeanPiece.at(ipiece);
	    pedestal_rms += MeanPiece.at(ipiece)*MeanPiece.at(ipiece);
	  }
	  else
	    dropped++;
	}

	int numsamples = 0;
	if(MeanPiece.size() - 1 - dropped != 0){
	  numsamples = MeanPiece.size() - 1 - dropped;
	  pedestal_mean = float(pedestal_mean)/float(numsamples);
	  pedestal_rms = sqrt(pedestal_rms/numsamples - (pedestal_mean*pedestal_mean));
	  pedestalerr = pedestal_rms/sqrt(numsamples);
	  hDroppedMean->SetBinContent(ichannel, float(dropped+1)/float(MeanPiece.size()));
	}
	else{
	  issues[4]++;
	  logfile << "Channel = " << ichannel << " has 0 windows." << std::endl;
	}
	dropped = 0;
	numsamples = 0;
	for (unsigned int ipiece=0; ipiece < RMSPiece.size() - 1; ++ipiece){
	  hRMSPiece[ichannel]->Fill(RMSPiece.at(ipiece));
	  if(abs(RMSPiece.at(ipiece) - MEAN_RMS) < 3*RMS_RMS) {
	    noise_mean += RMSPiece.at(ipiece);
	    noise_rms +=RMSPiece.at(ipiece)*RMSPiece.at(ipiece);
	  }
	  else
	    dropped++;
	}
	if(RMSPiece.size() - 1 - dropped != 0){
	  numsamples = RMSPiece.size() -1- dropped;
	  noise_mean = float(noise_mean)/float(numsamples);
	  noise_rms = sqrt(noise_rms/numsamples - (noise_mean*noise_mean));
	  noiseerr = noise_rms/sqrt(numsamples);
	  hDroppedRMS->SetBinContent(ichannel, float(dropped+1)/float(RMSPiece.size()));
	}
	else{
	  issues[5]++;
          logfile << "Channel = " << ichannel << " has 0 windows." << std::endl;
	}

	hRelSizes->SetBinContent(ichannel, float(RMSPiece.size()) / float(MeanPiece.size()));
	
      }//else ....means nostuck.size is > window
      
      hPed->Fill(pedestal_mean);
      hNoise->Fill(noise_mean);
      hPedChannel->SetBinContent(ichannel, pedestal_mean);
      hNoiseChannel->SetBinContent(ichannel, noise_mean);
      hDiffMean->Fill(mean-pedestal_mean);
      hDiffRMS->Fill(rms-noise_mean);
      hDiffMeanChannel->SetBinContent(ichannel, mean-pedestal_mean);
      hDiffRMSChannel->SetBinContent(ichannel, rms-noise_mean);
      
      if(abs(mean-pedestal_mean) > 10 or abs(rms-noise_mean) > 5){
	logfile << "Channel " << ichannel << " - Difference between methods:\n" 
		<< "mean = " << mean << ", pedestal = " << pedestal_mean
		<< ", rms = " << rms << ", noise = " << noise_mean << std::endl;
	pathology = true;
	issues[6]++;
      }
      if (rms == 0 or noise_mean == 0 ){
	logfile << "Channel " << ichannel << " - All Stuck ADCs\n"
		<< "mean = " << mean << ", pedestal = " << pedestal_mean
		<< ", rms = " << rms << ", noise = " << noise_mean << std::endl;
	pathology = true;
	issues[7]++;
      }
      if(rms > 20 or noise_mean > 20){
        logfile << "Channel " << ichannel << " - High RMS\n"
		<< "mean = " << mean << ", pedestal = " << pedestal_mean
		<< ", rms = " << rms << ", noise = " << noise_mean << std::endl;
	pathology = true;
	issues[8]++;
      }
      if(rms < 2 or noise_mean < 2){
	logfile << "Channel " << ichannel << " - Low RMS\n"
		<< "mean = " << mean << ", pedestal = " << pedestal_mean
		<< ", rms = " << rms << ", noise = " << noise_mean << std::endl;
	pathology = true;
	issues[11]++;
      }
      if(mean > 1000 or pedestal_mean > 1000){
        logfile << "Channel " << ichannel << " - High Pedestal\n"
                << "mean = " << mean << ", pedestal = " << pedestal_mean
                << ", rms = " << rms << ", noise = " << noise_mean << std::endl;
	pathology = true;
	issues[9]++;
      }
      if(mean < 200 or pedestal_mean < 200){
        logfile << "Channel " << ichannel << " - Low Pedestal\n"
                << "mean = " << mean << ", pedestal = " << pedestal_mean
                << ", rms = " << rms << ", noise = " << noise_mean << std::endl;
        pathology = true;
	issues[10]++;
      }
      
      ondatabase_file << ichannel << ", " << pedestal_mean << ", "
                  << noise_mean << ", " << pedestalerr << ", " << noiseerr 
		  << ", " << run << ", " << int(pathology) << std::endl;
      
      offdatabase_file << offchannels.at(ichannel) << ", " << pedestal_mean << ", "
		       << noise_mean << ", " << pedestalerr << ", " << noiseerr
		       << ", " << run << ", " << int(pathology) << std::endl;

      nosignal_file << Form("%4d",ichannel) << ", " << Form("%.1f", mean) << ", " << Form("%.1f", rms)
                    <<", " << Form("%.1f",meanerr)  << ", " << Form("%.1f",rmserr) << ", "
                    << run << ", " << int(pathology) << std::endl;      
      
      if(pathology){
	hADC[ichannel]->Write();
	hADCTotal[ichannel]->Write();
	hWaveform[ichannel]->Write();
        hMeanPiece[ichannel]->Write();
	hRMSPiece[ichannel]->Write();
        badlist << ichannel << std::endl;
      }

      if(pathology)
	badlist << ichannel << std::endl;
      
      MeanPiece.clear();
      RMSPiece.clear();
      
    } // channel loop
    
    for (int issue = 0; issue < numissues; ++issue)
      hIssues->Fill(issue,issues[issue]);
    
    hMean->Write();
    hRMS->Write();
    hPed->Write();
    hNoise->Write();
    hMeanChannel->Write();
    hRMSChannel->Write();
    hPedChannel->Write();
    hNoiseChannel->Write();
    hDroppedMean->Write();
    hDroppedRMS->Write();
    hDiffMean->Write();
    hDiffRMS->Write();
    hDiffMeanChannel->Write();
    hDiffRMSChannel->Write();
    hSizes->Write();
    hAllSizes->Write();
    hRelSizes->Write();
    hIssues->Write();

    std::cout << "Fraction of channels with more than 50% Stuck codes: " 
	      << 100-float(fifty_percent*100)/num_channels << "%" << std::endl;
    logfile << "Fraction of channels with more than 50% Stuck codes: " 
	    << 100-float(fifty_percent*100)/num_channels << "%" << std::endl;
    std::cout << "Average fraction of Stuck ADC values: " << average_stuck*100/num_channels << "%" << std::endl;
    logfile << "Average fraction of Stuck ADC values: " << average_stuck*100/num_channels << "%" << std::endl;

    logfile.close();
    badlist.close();
    return 1;
  }//if event has data!!
  else return 0;
}

void PedestalMonitoring::MonitoringPedestal::StartEvent(int eventNumber) {
  std::cout << "Event = " << eventNumber << std::endl;
  this->Reset();
}

void PedestalMonitoring::MonitoringPedestal::Reset() {
  fRCEADC.clear();
}
