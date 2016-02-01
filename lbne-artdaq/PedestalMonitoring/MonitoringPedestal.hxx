////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: MonitoringPedestal
// File: MonitoringPedestal.hxx
// Author:  Gabriel Santucci (gabriel.santucci@stonybrook.edu), Sept. 2015
//          based on 
//          Mike Wallbank's MonitoringData.hxx
//
// Class to handle the creation of monitoring Pedestal/Noise Runs.
// Owns all the data containers and has methods for filling with online data and for writing and
// saving the output in relevant places.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef MonitoringPedestal_hxx
#define MonitoringPedestal_hxx

#include <TH1.h>
#include <TH2.h>
#include <TProfile2D.h>
#include <TPad.h>
#include <TCanvas.h>
#include <TStyle.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TCollection.h>
#include <TSystemFile.h>
#include <TObjArray.h>
#include <TList.h>
#include <TROOT.h>
#include <TPaveText.h>
#include <TMath.h>
#include <TFile.h>
#include <TLegend.h>
#include <TTree.h>
#include <TF1.h>
#include <TVirtualFFT.h>


#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "PedestalMonitoringBase.cxx"
#include "DataReformatter.hxx"

class PedestalMonitoring::MonitoringPedestal {
public:

  void BeginMonitoring(int run, int subrun);
  void EndMonitoring();
  int RCEMonitoring(RCEFormatter const& rce_formatter, int window);
  void StartEvent(int eventNumber);
  void Reset();

private:

  int NumChannels = 2048;
  int numissues = 12;

  bool pathology;

  TH1I *hIssues;

  TH1I *hADC[2048];
  TH1I *hADCTotal[2048];
  TH1I *hWaveform[2048];
  
  TH1F *hMeanPiece[2048];
  TH1F *hRMSPiece[2048];

  TH1F *hMean;
  TH1F *hRMS;
  TH1F *hPed;
  TH1F *hNoise;
  TH1F *hDiffMean;
  TH1F *hDiffRMS;
  TH1F *hDiffMeanChannel;
  TH1F *hDiffRMSChannel;
  TH1F *hMeanChannel;
  TH1F *hRMSChannel;
  TH1F *hPedChannel;
  TH1F *hNoiseChannel;
  TH1F *hDroppedMean;
  TH1F *hDroppedRMS;
  TH1I *hSizes;
  TH1F *hAllSizes;
  TH1F *hRelSizes;

  double mean;
  double rms;
  double meanerr;
  double rmserr;

  double meantot;
  double rmstot;

  int run;
  int event;

  double pedestal_mean;
  double pedestal_rms;
  double noise_mean;
  double noise_rms;

  double pedestalerr;
  double noiseerr;

  ofstream ondatabase_file;
  ofstream offdatabase_file;
  ofstream nosignal_file;
  ofstream dumb_file;
  ofstream log_file;

  // File
  TFile *fDataFile;

  std::vector<std::vector<int> > fRCEADC;

};

#endif
