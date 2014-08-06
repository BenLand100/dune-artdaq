////////////////////////////////////////////////////////////////////////
// Class:       TpcMilliSliceDump
// Module Type: analyzer
// File:        TpcMilliSliceDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "art/Utilities/Exception.h"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>

namespace lbne {
  class TpcMilliSliceDump;
}

class lbne::TpcMilliSliceDump : public art::EDAnalyzer {
public:
  explicit TpcMilliSliceDump(fhicl::ParameterSet const & pset);
  virtual ~TpcMilliSliceDump();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
  std::string frag_type_;
};


lbne::TpcMilliSliceDump::TpcMilliSliceDump(fhicl::ParameterSet const & pset)
    : EDAnalyzer(pset),
      raw_data_label_(pset.get<std::string>("raw_data_label")),
      frag_type_(pset.get<std::string>("frag_type"))
{
}

lbne::TpcMilliSliceDump::~TpcMilliSliceDump()
{
}

void lbne::TpcMilliSliceDump::analyze(art::Event const & evt)
{
  art::EventNumber_t eventNumber = evt.event();

  // ***********************
  // *** TpcMilliSlice Fragments ***
  // ***********************

  // look for raw TpcMilliSlice data

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

      TpcMilliSliceFragment msf(frag);

      std::cout << std::endl;
      std::cout << "TpcMilliSlice fragment " << frag.fragmentID() << " consists of: " << msf.size() << " bytes containing "
                << msf.microSliceCount() << " microslices" << std::endl;
      std::cout << std::endl;

      for (uint32_t i_ms = 0; i_ms < msf.microSliceCount(); ++i_ms)
      {

    	  std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_ms);

    	  if (!microslice) {
    		  throw cet::exception("Error in TpcMilliSliceDump module: unable to find requested microslice");
    	  }

		  std::cout << "TpcMilliSlice fragment " << frag.fragmentID() << ", microslice " << i_ms
				    << " has sequence ID " << microslice->sequence_id()
				    << " timestamp " << microslice->timestamp()
			        << " and consists of: " << std::endl;
		  std::cout << "  " << microslice->nanoSliceCount() << " nanoslices" << " for each of " << (int)microslice->channelGroupCount()
				    << " groups in " << microslice->size() << " bytes" << std::endl;
		  std::cout << "  RecFormat: "   << (int)microslice->record_format()
		            << " RecType: "     << (int)microslice->record_type()
                    << " DataFormat: "  << (int)microslice->data_format()
                    << " TriggerType: " << (int)microslice->trigger_type()
                    << " SubType: "     << (int)microslice->data_subtype() << std::endl;
		  std::cout << "  Group Physical IDs: ";
		  for (uint8_t i_group = 0; i_group < microslice->channelGroupCount(); i_group++)
		  {
			  std::cout << microslice->groupPhysicalId(i_group) << " ";
		  }

		  std::cout << "\n" << std::endl;

		  for (uint32_t i_nano = 0; i_nano < microslice->nanoSliceCount(); i_nano+=(microslice->nanoSliceCount()-1))
		  {
			  for (uint8_t i_group = 0; i_group < microslice->channelGroupCount(); i_group++)
			  {
				std::unique_ptr<const TpcNanoSlice> nanoslice = microslice->nanoSlice(i_group, i_nano);
				if (! nanoslice )
				{
					throw cet::exception("Error in TpcMilliSliceDump module: Unable to find requested nanoslice");
				}

				std::cout << "  NanoSlice " << i_nano << " : group " << (int)i_group << " size " << nanoslice->size()
						  << " bytes " << nanoslice->sampleCount() << " samples: ";


				for (uint32_t i_samp= 0; i_samp < 6; i_samp++)
				{
					uint16_t val = std::numeric_limits<uint16_t>::max();
					bool success = nanoslice->sampleValue(i_samp, val);

					if (!success)
					{
						throw cet::exception("Error in TpcMilliSliceDump module: Unable to find requested sample value in nanoslice");
					}
					std::cout << val << " ";
				}
				std::cout << " ..." << std::endl;

			  }
			  std::cout << std::endl;
		  }
    	  if (i_ms >= 4) break;
      }

      // In case we want to examine the header "invasively" :
      //      const TpcMilliSlice::Header* header = reinterpret_cast<const TpcMilliSlice::Header*>( frag.dataBeginBytes() );
    }
  }
  else {
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
              << ", event " << eventNumber << " has zero"
              << " TpcMilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;

}

DEFINE_ART_MODULE(lbne::TpcMilliSliceDump)
