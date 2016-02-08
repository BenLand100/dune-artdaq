////////////////////////////////////////////////////////////////////////////////////////////////////
// Pedestal Monitoring namespace
// Author: Gabriel Santucci, gabriel.santucci@stonybrook.edu, based on Mike's OnlineMonitor:
// Mike Wallbank, m.wallbank@sheffield.ac.uk (September 2015)
//
// Contains global variables used by classes in the pedestal monitoring namespace.
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef PedestalMonitoringNamespace
#define PedestalMonitoringNamespace

#include "TString.h"

namespace PedestalMonitoring {

  class OnlinePedestal;
  class RCEFormatter;
  class MonitoringPedestal;

  const unsigned int NRCEChannels = 2048;
  const unsigned int NAPAChannels = 512;
  const unsigned int NAPA = 4;
  const unsigned int NPlanes = 3;

  const TString HistSavePath  = "/data/lbnedaq/scratch/santucci/pedestalruns/";

}

#endif
