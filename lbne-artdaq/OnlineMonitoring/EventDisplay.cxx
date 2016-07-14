////////////////////////////////////////////////////////////////////////////////////////////////////
// Class: EventDisplay
// File: EventDisplay.cxx
// Author: Mike Wallbank (m.wallbank@sheffield.ac.uk), September 2015
//
// Class with methods for making online event displays.
////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EventDisplay.hxx"

void OnlineMonitoring::EventDisplay::MakeEventDisplay(RCEFormatter const& rceformatter, ChannelMap const& pedestalMap, int collectionPedestal, double driftVelocity) {

  /// Makes crude online event display and saves it as an image to be displayed on the web

  // double cmToWire = 10./4.5;
  // double cmToTick = 10 * driftVelocity / 2;
  //fEVD = new TH2D("EVD",";z (cm);x (cm)",(EVD::UpperZ-EVD::LowerZ)*cmToWire,EVD::LowerZ,EVD::UpperZ,(EVD::UpperX-EVD::LowerX)*cmToTick,EVD::LowerX,EVD::UpperX);
  if (driftVelocity > 10)
    std::cout << std::endl;

  fEVD = new TH2D("EVD",";Collection wire;Tick",344,0,344,7000,-1000,6000);

  const std::vector<std::vector<int> > ADCs = rceformatter.EVDADCVector();
  //double x,z;
  int drift, apa, rce, collectionChannel, ADC, pedestal = 0;

  // Loop over channels
  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {

    if (!ADCs.at(channel).size())
      continue;

    // Only consider collection plane
    if (fChannelMap->PlaneFromOnlineChannel(channel) != 2) continue;

    // Find properties of this channel
    rce = fChannelMap->RCEFromOnlineChannel(channel);
    if (rce == 0 or rce == 1 or rce == 4 or rce == 5 or rce == 8 or rce == 9 or rce == 12 or rce == 13) drift = 1;
    else drift = 0;
    apa = fChannelMap->APAFromOnlineChannel(channel);
    if (pedestalMap.hasPedestalData())
      pedestal = pedestalMap.GetPedestal(channel);
    else
      pedestal = collectionPedestal;
    collectionChannel = GetCollectionChannel(fChannelMap->Offline(channel), apa, drift);
    //z = GetZ(collectionChannel);

    // Loop over ticks
    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {

      // Correct for pedestal
      ADC = ADCs.at(channel).at(tick) - pedestal;

      // If in certain range fill event display (range will be limited later...)
      if (ADC > -1000 and ADC < 1000) {

	// // Subtract pedestal again for one of the small APAs (they overlap)
	// if (apa == 3) ADC -= collectionPedestal;

	// Set limits on the filled charge!
	ADC = TMath::Min(ADC, 250);
	ADC = TMath::Max(ADC, -100);

	// x = tick * driftVelocity / 2;
	// x /= 10;
	if (drift == 0) fEVD->Fill(collectionChannel,(int)-tick,ADC);
	if (drift == 1) fEVD->Fill(collectionChannel,tick,ADC);

      } // evd range

    } // ticks

  } // channels

}

void OnlineMonitoring::EventDisplay::SaveEventDisplay(int run, int subrun, int event, TString const& evdSavePath) {

  /// Saves the online event display in the specified directory and sets up info for the cron job to sync

  // Save the event display and make it look pretty

  // Get time of saving
  time_t rawtime;
  struct tm* timeinfo;
  char buffer[80];
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(buffer,80,"%B %d, %Y, %R",timeinfo);
  std::string time = std::string(buffer);

  // Want a black and white colour scale
  Double_t RedBW[2] = { 1.00, 0.00 };
  Double_t GreenBW[2] = { 1.00, 0.00 };
  Double_t BlueBW[2] = { 1.00, 0.00 };
  Double_t LengthBW[2] = { 0.00, 1.00 };
  TColor::CreateGradientColorTable(2, LengthBW, RedBW, GreenBW, BlueBW, 1000);

  // Make canvas to save on
  TCanvas* evdCanvas = new TCanvas("","",1600,1800);
  fEVD->GetZaxis()->SetRangeUser(-100,250);
  fEVD->GetXaxis()->SetLabelSize(0.025);
  fEVD->GetYaxis()->SetLabelSize(0.025);
  fEVD->GetZaxis()->SetLabelSize(0.025);
  fEVD->GetXaxis()->SetTitleSize(0.025);
  fEVD->GetYaxis()->SetTitleSize(0.025);
  fEVD->GetZaxis()->SetTitleSize(0.025);
  fEVD->GetYaxis()->SetTitleOffset(2);

  // Draw EVD
  fEVD->Draw("colz");

  // Draw lines
  TLine DriftLine, APALine;
  DriftLine.SetLineStyle(2);
  DriftLine.SetLineWidth(6);
  DriftLine.SetLineColor(4);
  APALine.SetLineStyle(7);
  APALine.SetLineWidth(4);
  APALine.SetLineColor(2);
  //line.DrawLine(EVD::LowerZ,0,EVD::UpperZ,0);
  DriftLine.DrawLine(0,0,344,0);
  APALine.DrawLine(112,-1000,112,6000);
  APALine.DrawLine(116,-1000,116,6000);
  APALine.DrawLine(228,-1000,228,6000);
  APALine.DrawLine(232,-1000,232,6000);
  
  // Draw text
  std::stringstream evdText;
  evdText << "Run " << run << ", Subrun " << subrun << ": Event " << event;
  TLatex text;
  text.SetTextSize(0.02);
  text.DrawLatex(5,5800,"#it{DUNE 35t Event Display}");
  text.DrawLatex(5,5600,evdText.str().c_str());
  text.DrawLatex(5,5400,time.c_str());

  // Save the image
  evdCanvas->SaveAs(evdSavePath+TString("evd")+TString(".png"));//+ImageType);

  // Add event file
  std::ofstream tmp((evdSavePath+TString("event")).Data());
  tmp << run << " " << subrun << " " << event;
  tmp.flush();
  tmp.close();

  delete evdCanvas;
  gStyle->SetPalette(1);

  mf::LogInfo("Monitoring") << "New event display for event " << event << " is viewable at http://lbne-dqm.fnal.gov/EventDisplay.";

  return;

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
    collectionChannel += 4;
    break;

  case 2:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 1200;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 1312;
    collectionChannel += 4;
    break;

  case 3:
    if (drift == 0) collectionChannel = offlineCollectionChannel - 1600;
    if (drift == 1) collectionChannel = offlineCollectionChannel - 1712;
    collectionChannel += 8;
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
