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

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include "OnlineMonitoringBase.cxx"
#include "DataReformatter.hxx"

class OnlineMonitoring::MonitoringPedestal {
public:

  void BeginMonitoring(int run, int subrun);
  void EndMonitoring();
  void FillTree(RCEFormatter const& rce_formatter);
  void GeneralMonitoring();
  void RCEMonitoring(RCEFormatter const& rce_formatter);
  void GetPedestal(std::vector<int> ADCs, int channel, double mean, double rms);
  void MakeHistograms();
  void StartEvent(int eventNumber, bool maketree);
  void Reset();
  void WriteMonitoringPedestal(int run, int subrun);

private:

  int fEventNumber;

  // File
  TFile *fDataFile;

  // Tree
  bool fMakeTree;
  TTree *fDataTree;

  // Histograms
  TObjArray fHistArray;
  std::map<std::string,std::string> fFigureCaptions;
  TCanvas *fCanvas;

  // RCE
  TH1I *hTotalADCEvent, *hTotalRCEHitsEvent, *hTotalRCEHitsChannel, *hTimesADCGoesOverThreshold,  *hNumMicroslicesInMillislice, *hNumNanoslicesInMicroslice, *hNumNanoslicesInMillislice;
  TH2I *hRCEBitCheckAnd, *hRCEBitCheckOr;
  TH1D *hAvADCAllMillislice;
  TH2D *hAvADCChannelEvent;
  TProfile *hADCMeanChannelAPA1, *hADCMeanChannelAPA2, *hADCMeanChannelAPA3, *hADCMeanChannelAPA4, *hADCRMSChannelAPA1, *hADCRMSChannelAPA2, *hADCRMSChannelAPA3, *hADCRMSChannelAPA4;
  TProfile *hRCEDNoiseChannel, *hAsymmetry, *hLastSixBitsCheckOff, *hLastSixBitsCheckOn;
  std::map<int,TProfile*> hADCChannel, hADCChannelFilt;
  std::map<int,TH1D*> hAvADCMillislice;
  std::map<int,TH1D*> hAvADCMillisliceChannel;
  std::map<int,TH1D*> hDebugChannelHists;

  //Calibration                                   
  TF1 *mygaus;

  std::map<int,TH1F*> hADCUnfiltered;
  std::map<int,TH1F*> hADCFiltered;
  TProfile *hPedestalChan;
  TProfile *hNoiseChan;
  TH1F *hPedestal;
  TH1F *hNoise;
  TH1F *hMeanDiff;
  TH1F *hSigmaDiff;
  TH1F *hMeanFiltDiff;
  TH1F *hSigmaFiltDiff;

  // General
  TH1I *hNumSubDetectorsPresent, *hSizeOfFiles;
  TH1D *hSizeOfFilesPerEvent;

  // Variables for monitoring
  int fTotalADC;
  int fTotalRCEHitsEvent;
  int fTimesADCGoesOverThreshold;
  int fNumSubDetectorsPresent;
  std::vector<std::vector<int> > fRCEADC;

  bool checkedFileSizes;
  int fThreshold = 10;
  bool fIsInduction = true;

  TString HistSaveDirectory;

  // Run options
  bool _interestingchannelsfilled = false;

};

#endif
