//compile independently with: g++ -o ChannelMapTree ChannelMapTree.cc `root-config --cflags --glibs`

#include "TROOT.h"
#include "TFile.h"
#include "TTree.h"

#include<iostream>
#include <string.h>

void MakeChannelMap(TString map){

  TFile *f = new TFile("channelmap.root","RECREATE");
  TTree *T = new TTree("channeltree","");
  Long64_t nlines = T->ReadFile(map,"channel/I:rce/I:rcechannel/I:apa/I:plane/I:offchannel/I");
  T->Write();
  f->Close();

  std::cout << "\nMakeChannelMap found " << nlines << " channels!" << std::endl;

}

int main(int argc, const char *argv[]){

  std::string map_path;
  map_path = "/data/dunedaq/scratch/santucci/commit/dune-artdaq-base/dune-artdaq/dune-artdaq/PedestalMonitoring/MakeChannelMap/detailedMap.txt";

  if(argc==2)
    map_path = argv[1];

  MakeChannelMap(map_path);

}

