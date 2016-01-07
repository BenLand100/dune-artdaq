////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: EventDisplay
// File: EventDisplay.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class with methods for making online event displays.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EventDisplay.hxx"

void OnlineMonitoring::EventDisplay::MakeEventDisplay(RCEFormatter const& rceformatter,
						      ChannelMap const& channelMap,
						      double driftVelocity,
						      int event,
						      TString const& evdSavePath) {

  /// Makes crude online event display and saves it as an image to be displayed on the web

  TH2D* EVD = new TH2D("EVD",";z (cm);x (cm)",EVD::UpperZ-EVD::LowerZ,EVD::LowerZ,EVD::UpperZ,EVD::UpperX-EVD::LowerX,EVD::LowerX,EVD::UpperX);

  const std::vector<std::vector<int> > ADCs = rceformatter.EVDADCVector();
  double x,z;

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {
    if (!ADCs.at(channel).size())
      continue;
    // Only consider collection plane
    if (channelMap.GetPlane(channel) != 2) continue;
    int drift = channelMap.GetDriftVolume(channel);
    int collectionChannel = GetCollectionChannel(channelMap.GetOfflineChannel(channel), channelMap.GetAPA(channel), drift);
    z = GetZ(collectionChannel);
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
      int ADC = ADCs.at(channel).at(tick);
      x = tick * driftVelocity / 2;
      x /= 10;
      if (drift == 0) EVD->Fill(z,(int)-x,ADC);
      if (drift == 1) EVD->Fill(z,x,ADC);
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
  line.DrawLine(EVD::LowerZ,0,EVD::UpperZ,0);
  evdCanvas->SaveAs(evdSavePath+TString("evd.png"));//+ImageType);

  // Add event file
  ofstream tmp((evdSavePath+TString("event")).Data());
  tmp << event;
  tmp.flush();
  tmp.close();

  delete evdCanvas; delete EVD;

  mf::LogInfo("Monitoring") << "New event display for event " << event << " is viewable at http://lbne-dqm.fnal.gov/EventDisplay.";

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

double OnlineMonitoring::EventDisplay::GetZ(int collectionChannel) {

  /// Finds the z coordinate of a global collection channel

  // The specific numbers will have to be hard-coded but they will not change for the 35t
  // The spacing between collection plane wires is 4.5mm and the gaps between the APAs is 113.142mm
  // There are 112 collection wires on each side of an APA

  double z = (collectionChannel * 4.5) + (113.142 * (int)(collectionChannel/(double)112));

  // Convert to cm
  z /= 10;

  return z;

}
