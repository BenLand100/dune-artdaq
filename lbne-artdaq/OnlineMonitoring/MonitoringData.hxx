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
#include <map>
#include <algorithm>

#include "OnlineMonitoringBase.cxx"
#include "DataReformatter.hxx"

class OnlineMonitoring::MonitoringData {
public:

  void BeginMonitoring(int run, int subrun);
  void EndMonitoring();
  void FillTree(RCEFormatter const& rce_formatter, SSPFormatter const& ssp_formatter);
  void GeneralMonitoring(RCEFormatter const& rceformatter, SSPFormatter const& sspformatter, PTBFormatter const& ptbformatter);
  void GeneralMonitoring();
  void RCEMonitoring(RCEFormatter const& rce_formatter);
  void SSPMonitoring(SSPFormatter const& ssp_formatter);
  void PTBMonitoring(PTBFormatter const& ptb_formatter);
  void MakeHistograms();
  void StartEvent(int eventNumber, bool maketree);
  void WriteMonitoringData(int run, int subrun);

private:

  int fEventNumber;

  // Data handling
  TFile* fDataFile;
  bool fMakeTree;
  TTree* fDataTree;
  TString HistSaveDirectory;
  TObjArray fHistArray;
  std::map<std::string,std::string> fFigureCaptions;
  TCanvas* fCanvas;

  std::vector<std::vector<int> > fRCEADC, fSSPADC;

  bool filledRunData;

  // crap to sort out
  int fThreshold = 10;
  bool fIsInduction = true;
  bool _interestingchannelsfilled = false;

  // Monitoring Data --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  // RCE
  TH1I *hTotalADCEvent, *hTotalRCEHitsEvent, *hTotalRCEHitsChannel, *hTimesADCGoesOverThreshold,  *hNumMicroslicesInMillislice, *hNumNanoslicesInMicroslice, *hNumNanoslicesInMillislice;
  TH2I *hRCEBitCheckAnd, *hRCEBitCheckOr;
  TH2D *hAvADCChannelEvent;
  TProfile *hADCMeanChannelAPA1, *hADCMeanChannelAPA2, *hADCMeanChannelAPA3, *hADCMeanChannelAPA4, *hADCRMSChannelAPA1, *hADCRMSChannelAPA2, *hADCRMSChannelAPA3, *hADCRMSChannelAPA4;
  TProfile *hRCEDNoiseChannel, *hAsymmetry, *hLastSixBitsCheckOff, *hLastSixBitsCheckOn;
  std::map<int,TProfile*> hADCChannel;
  std::map<int,TH1D*> hAvADCMillislice;
  std::map<int,TH1D*> hAvADCMillisliceChannel;
  std::map<int,TH1D*> hDebugChannelHists;

  // SSP
  TProfile *hWaveformMean, *hWaveformRMS, *hWaveformPeakHeight, *hWaveformIntegral, *hWaveformIntegralNorm, *hWaveformPedestal, *hWaveformNumTicks, *hNumberOfTriggers;

  // PTB
  TProfile *hPTBTriggerRates;
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

  // General
  TH1I *hNumSubDetectorsPresent, *hSizeOfFiles, *hSubDetectorsWithData;
  TH1D *hSizeOfFilesPerEvent;

};

#endif
