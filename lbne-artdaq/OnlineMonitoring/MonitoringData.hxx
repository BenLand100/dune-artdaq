////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: MonitoringData
// File: MonitoringData.hxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class to handle the creation of monitoring data.
// Owns all the data containers and has methods for filling with online data and for writing and
// saving the output in relevant places.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef MonitoringData_hxx
#define MonitoringData_hxx

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

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <sstream>
#include <fstream>

#include "OnlineMonitoringNamespace.cxx"
#include "DataReformatter.hxx"

class OnlineMonitoring::MonitoringData {
public:

  void BeginMonitoring(int run, int subrun);
  void EndMonitoring();
  void FillTree(RCEFormatter const& rce_formatter, SSPFormatter const& ssp_formatter);
  void GeneralMonitoring();
  void RCEMonitoring(RCEFormatter const& rce_formatter);
  void SSPMonitoring(SSPFormatter const& ssp_formatter);
  void PTBMonitoring(PTBFormatter const& ptb_formatter);
  void AddHists();
  void StartEvent(int eventNumber, bool maketree);
  void Reset();
  void WriteMonitoringData(int run, int subrun);

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
  TProfile *hADCMeanChannel, *hADCRMSChannel, *hRCEDNoiseChannel, *hAsymmetry, *hLastSixBitsCheckOff, *hLastSixBitsCheckOn;
  std::map<int,TProfile*> hADCChannel;
  std::map<int,TH1D*> hAvADCMillislice;
  std::map<int,TH1D*> hAvADCMillisliceChannel;
  std::map<int,TH1D*> hDebugChannelHists;

  // SSP
  TH1I *hTotalWaveformEvent, *hTotalSSPHitsEvent, *hTotalSSPHitsChannel, *hTimesWaveformGoesOverThreshold;
  TH2I *hSSPBitCheckAnd, *hSSPBitCheckOr;
  TH2D *hAvWaveformChannelEvent;
  TProfile *hWaveformMeanChannel, *hWaveformRMSChannel, *hSSPDNoiseChannel;
  std::map<int,TProfile*> hWaveformChannel;

  // PTB
  TProfile *hPTBTSUCounterHitRateWU,    *hPTBTSUCounterActivationTimeWU;
  TProfile *hPTBTSUCounterHitRateEL,    *hPTBTSUCounterActivationTimeEL;
  TProfile *hPTBTSUCounterHitRateExtra, *hPTBTSUCounterActivationTimeExtra;
  TProfile *hPTBTSUCounterHitRateNU,    *hPTBTSUCounterActivationTimeNU;
  TProfile *hPTBTSUCounterHitRateSL,    *hPTBTSUCounterActivationTimeSL;
  TProfile *hPTBTSUCounterHitRateNL,    *hPTBTSUCounterActivationTimeNL;
  TProfile *hPTBTSUCounterHitRateSU,    *hPTBTSUCounterActivationTimeSU;
  TProfile *hPTBBSUCounterHitRateRM,    *hPTBBSUCounterActivationTimeRM;
  TProfile *hPTBBSUCounterHitRateCU,    *hPTBBSUCounterActivationTimeCU;
  TProfile *hPTBBSUCounterHitRateCL,    *hPTBBSUCounterActivationTimeCL;
  TProfile *hPTBBSUCounterHitRateRL,    *hPTBBSUCounterActivationTimeRL;
  TProfile *hPTBTriggerRates;

  // General
  TH1I *hNumSubDetectorsPresent, *hSizeOfFiles;
  TH1D *hSizeOfFilesPerEvent;

  // Variables for monitoring
  int fTotalADC, fTotalWaveform;
  int fTotalRCEHitsEvent, fTotalSSPHitsEvent;
  int fTimesADCGoesOverThreshold, fTimesWaveformGoesOverThreshold;
  int fNumSubDetectorsPresent;
  std::vector<std::vector<int> > fRCEADC, fSSPADC;

  bool checkedFileSizes;
  int fThreshold = 10;
  bool fIsInduction = true;

  // Run options
  bool _interestingchannelsfilled = false;

};

#endif
