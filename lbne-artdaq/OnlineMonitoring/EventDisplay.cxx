////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: EventDisplay
// File: EventDisplay.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class with methods for making online event displays.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EventDisplay.hxx"

void OnlineMonitoring::EventDisplay::MakeEventDisplay(RCEFormatter const& rceformatter, ChannelMap const& channelMap, int event) {

  /// Makes an event display and saves it as an image to be uploaded

  TH2D* UDisplay = new TH2D("UDisplay",";Wire;Tick;",2048,0,2048,32000,0,32000);
  TH2D* VDisplay = new TH2D("VDisplay",";Wire;Tick;",2048,0,2048,32000,0,32000);
  TH2D* ZDisplay = new TH2D("ZDisplay",";Wire;Tick;",2048,0,2048,32000,0,32000);

  const std::vector<std::vector<int> > ADCs = rceformatter.ADCVector();

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
    int offlineChannel = channelMap.GetOfflineChannel(channel);
    int plane = channelMap.GetPlane(channel);
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
      int ADC = ADCs.at(channel).at(tick);
      if (plane == 0) UDisplay->Fill(offlineChannel,tick,ADC);
      if (plane == 1) VDisplay->Fill(offlineChannel,tick,ADC);
      if (plane == 2) ZDisplay->Fill(offlineChannel,tick,ADC);
    }
  }

  // Save the event display and make it look pretty
  TCanvas* evdCanvas = new TCanvas();
  evdCanvas->Divide(1,3);
  evdCanvas->cd(1);
  ZDisplay->Draw("colz");
  evdCanvas->cd(2);
  VDisplay->Draw("colz");
  evdCanvas->cd(3);
  UDisplay->Draw("colz");
  evdCanvas->SaveAs(EVDSavePath+TString("evd.eps"));//+ImageType);

  // Add run file
  ofstream tmp((EVDSavePath+TString("event")).Data());
  tmp << event;
  tmp.flush();
  tmp.close();
  system(("chmod -R a=rwx "+std::string(EVDSavePath+TString("event"))).c_str());  

  delete evdCanvas; delete UDisplay; delete VDisplay; delete ZDisplay;

}

