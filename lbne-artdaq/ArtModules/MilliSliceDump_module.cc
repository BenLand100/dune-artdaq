////////////////////////////////////////////////////////////////////////
// Class:       MilliSliceDump
// Module Type: analyzer
// File:        MilliSliceDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Utilities/Exception.h"
#include "lbne-artdaq/Overlays/MilliSliceFragment.hh"
#include "artdaq/DAQdata/Fragments.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>

namespace lbne {
  class MilliSliceDump;
}

class lbne::MilliSliceDump : public art::EDAnalyzer {
public:
  explicit MilliSliceDump(fhicl::ParameterSet const & pset);
  virtual ~MilliSliceDump();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
  std::string frag_type_;
};


lbne::MilliSliceDump::MilliSliceDump(fhicl::ParameterSet const & pset)
    : EDAnalyzer(pset),
      raw_data_label_(pset.get<std::string>("raw_data_label")),
      frag_type_(pset.get<std::string>("frag_type"))
{
}

lbne::MilliSliceDump::~MilliSliceDump()
{
}

void lbne::MilliSliceDump::analyze(art::Event const & evt)
{
  art::EventNumber_t eventNumber = evt.event();

  // ***********************
  // *** MilliSlice Fragments ***
  // ***********************

  // look for raw MilliSlice data

  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);

  if (raw.isValid()) {
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has " << raw->size()
              << " fragment(s) of type " << frag_type_ << std::endl;

    for (size_t idx = 0; idx < raw->size(); ++idx) {
      const auto& frag((*raw)[idx]);

      MilliSliceFragment msf(frag);

      std::cout << std::endl;
      std::cout << "MilliSlice fragment " << frag.fragmentID() << " consists of: " << std::endl;
      std::cout << msf.size() << " bytes" << std::endl;
      std::cout << msf.microSliceCount() << " microslices" << std::endl;
      std::cout << std::endl;

      for (uint32_t i_ms = 0; i_ms <  msf.microSliceCount(); ++i_ms) {

	std::unique_ptr<const MicroSlice> microslice = msf.microSlice( i_ms );

	if (! microslice ) {
	  throw cet::exception("Error in MilliSliceDump module: Unable to find requested microslice");
	}

	std::cout << "MilliSlice fragment " << frag.fragmentID() << ", microslice " << i_ms << 
	  " consists of: " << std::endl;
	std::cout << microslice->size() << " bytes" << std::endl;
	std::cout << microslice->nanoSliceCount() << " nanoslices" << std::endl;

	if (microslice->nanoSliceCount() > 0) {

	  std::unique_ptr<const NanoSlice> nanoslice = microslice->nanoSlice(0);

	  if (! nanoslice ) {
	    throw cet::exception("Error in MilliSliceDump module: Unable to find requested nanoslice");
	  }

	  std::cout << "First nanoslice: ";

	  for (uint32_t i_samp = 0; i_samp < nanoslice->sampleCount(); ++i_samp) {
	    uint16_t val = std::numeric_limits<uint16_t>::max();
	    bool success = nanoslice->sampleValue(i_samp, val);
	    
	    if (!success) {
	      throw cet::exception("Error in MilliSliceDump module: Unable to find requested sample value in nanoslice");
	    }

	    std::cout << val << " ";
	  }
	  std::cout << "\n" << std::endl; // Double newline
	}
      }

      // In case we want to examine the header "invasively" :
      //      const MilliSlice::Header* header = reinterpret_cast<const MilliSlice::Header*>( frag.dataBeginBytes() );
    }
  }
  else {
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " MilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;

}

DEFINE_ART_MODULE(lbne::MilliSliceDump)
