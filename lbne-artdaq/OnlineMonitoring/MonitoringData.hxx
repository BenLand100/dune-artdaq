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

// ROOT
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
#include <TLine.h>
#include <TFrame.h>
#include <TGraph.h>
#include <TMultiGraph.h>

// C++
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>
#include <algorithm>
#include <ctime>
#include <time.h>

// framework
#include "messagefacility/MessageLogger/MessageLogger.h"

// monitoring
#include "OnlineMonitoringBase.hxx"
#include "DataReformatter.hxx"

class OnlineMonitoring::MonitoringData {
public:

  void BeginMonitoring(int run, int subrun, TString const& monitorSavePath, bool detailedMonitoring, bool scopeMonitoring);
  void EndMonitoring();
  void FillTree(RCEFormatter const& rce_formatter, SSPFormatter const& ssp_formatter);
  void GeneralMonitoring(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter, PTBFormatter const& ptbformatter, TString const& dataDirPath);
  void RCEMonitoring(RCEFormatter const& rce_formatter, int event);
  void RCEDetailedMonitoring(RCEFormatter const& rce_formatter);
  void RCEScopeMonitoring(RCEFormatter const& rce_formatter, int event);
  void RCELessFrequentMonitoring(RCEFormatter const& rce_formatter);
  void SSPMonitoring(SSPFormatter const& ssp_formatter);
  void PTBMonitoring(PTBFormatter const& ptb_formatter);
  void WriteMonitoringData(int run, int subrun, int eventsProcessed, TString const& imageType);

private:

  void ConstructTimingSyncGraphs();
  void MakeHistograms();
  void MakeDetailedHistograms();
  void MakeScopeHistograms();

  // Data handling
  TFile*    fDataFile;
  TTree*    fDataTree;
  TCanvas*  fCanvas;
  TString   fHistSaveDirectory;
  TObjArray fHistArray;

  bool fDetailedMonitoring;
  bool fFilledRunData;
  std::string fRunStartTime;

  std::map<std::string,std::string> fFigureCaptions;
  std::map<std::string,TLegend*> fFigureLegends;

  std::vector<std::vector<int> > fRCEADC, fSSPADC;

  // Monitoring Data ---------------------------------------------------------------------------------------------------------------------------------------------------------
  // General
  TH1I *hNumSubDetectorsPresent, *hSizeOfFiles, *hSubDetectorsWithData, *hSubDetectorsPresent;
  TH1D *hSizeOfFilesPerEvent;
  TGraph *hTimeSyncSSPs[NSSPs], *hTimeSyncAverageSSPs[NSSPs];
  TMultiGraph *hSSPTimeSync, *hSSPTimeSyncAverage;

  // RCE
  TH1I *hTotalADCEvent, *hTotalRCEHitsEvent, *hTotalRCEHitsChannel, *hTimesADCGoesOverThreshold,  *hNumMicroslicesInMillislice, *hNumNanoslicesInMicroslice, *hNumNanoslicesInMillislice;
  TH2I *hRCEBitCheckAnd, *hRCEBitCheckOr;
  TH2D *hAvADCChannelEvent, *hADCChannel, *hTickRatioChannel, *hTickTotalChannel;
  TProfile *hADCMeanChannelAPA1, *hADCMeanChannelAPA2, *hADCMeanChannelAPA3, *hADCMeanChannelAPA4, *hADCRMSChannelAPA1, *hADCRMSChannelAPA2, *hADCRMSChannelAPA3, *hADCRMSChannelAPA4;
  TProfile2D *hFFTChannelRCE00;
  TProfile *hRCEDNoiseChannel, *hAsymmetry, *hLastSixBitsCheckOff, *hLastSixBitsCheckOn;
  std::map<int,TProfile*> hADCChannelMap;
  std::map<int,TH1D*> hAvADCMillislice;
  std::map<int,TH1D*> hAvADCMillisliceChannel;
  std::map<int,TH1D*> hDebugChannelHists;
  // Scope
  TH1F *hScopeTraceFFT1s;
  TProfile *hScopeTrace1s;

  // SSP
  TProfile *hWaveformMean, *hWaveformRMS, *hWaveformPeakHeight, *hWaveformIntegral, *hWaveformIntegralNorm, *hWaveformPedestal, *hWaveformNumTicks, *hTriggerFraction;
  TH1I *hNumberOfTriggers;

  // PTB
  TProfile *hPTBTriggerRates;
  TH1I *hPTBBlockLength, *hPTBPayloadType;
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

};

#endif
