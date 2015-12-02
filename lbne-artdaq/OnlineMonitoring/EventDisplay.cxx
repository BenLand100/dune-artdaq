////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: EventDisplay
// File: EventDisplay.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class with methods for making online event displays.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EventDisplay.hxx"

void OnlineMonitoring::EventDisplay::MakeEventDisplay(RCEFormatter const& rceformatter, ChannelMap const& channelMap, int event, TString const& evdSavePath) {

  /// Makes crude online event display and saves it as an image to be displayed on the web

  //TH2D* EVD = new TH2D("EVD","",336,0,336,35000,-30000,5000);
  TH2D* EVD = new TH2D("EVD","",336,0,336,3500,-3000,500);

  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
    // Only consider collection plane
    if (channelMap.GetPlane(channel) != 2) continue;
    int drift = channelMap.GetDriftVolume(channel);
    int collectionChannel = GetCollectionChannel(channelMap.GetOfflineChannel(channel), channelMap.GetAPA(channel), drift);
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
      int ADC = ADCs.at(channel).at(tick);
      if (drift == 0) EVD->Fill(collectionChannel,tick,ADC);
      if (drift == 1) EVD->Fill(collectionChannel,(int)-tick,ADC);
    }
  }

  // Save the event display and make it look pretty
  // Double_t Red[2] = { 1.00, 0.00 };
  // Double_t Green[2] = { 1.00, 0.00 };
  // Double_t Blue[2] = { 1.00, 0.00 };
  // Double_t Length[2] = { 0.00, 1.00 };
  // TColor::CreateGradientColorTable(2, Length, Red, Green, Blue, 1000);
  TCanvas* evdCanvas = new TCanvas();
  EVD->Draw("colz");
  TLine line;
  line.SetLineStyle(2);
  line.SetLineWidth(4);
  line.DrawLine(0,0,336,0);
  evdCanvas->SaveAs(evdSavePath+TString("evd.png"));//+ImageType);

  // Add event file
  ofstream tmp((evdSavePath+TString("event")).Data());
  tmp << event;
  tmp.flush();
  tmp.close();

  delete evdCanvas; delete EVD;

}

int OnlineMonitoring::EventDisplay::GetCollectionChannel(int offlineCollectionChannel, int apa, int drift) {

  /// Takes a collection channel in a particular drift volume on a particular APA and returns a global collection plane channel number

  // These numbers are going to have to be hard-coded in but won't change for the 35t
  // There are 112 collection wires on each side of an APA

  int collectionChannel = 0;

  switch (apa) {

  case 0:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 288;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 400;
    break;

  case 1:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 688;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 800;
    break;

  case 2:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 1200;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 1312;
    break;

  case 3:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 1600;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 1712;
    break;

  }

  return collectionChannel;

}
