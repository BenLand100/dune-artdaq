////////////////////////////////////////////////////////////////////////
// Class:       PennMilliSliceDump
// Module Type: analyzer
// File:        PennMilliSliceDump_module.cc
// Description: Prints out information about each event.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "canvas/Utilities/Exception.h"
#include "dune-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"
#include "dune-artdaq/Generators/pennBoard/PennCompileOptions.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>
#include <bitset>

using namespace dune;

namespace dune {
class PennMilliSliceDump;
}

class dune::PennMilliSliceDump : public art::EDAnalyzer {
public:
	explicit PennMilliSliceDump(fhicl::ParameterSet const & pset);
	virtual ~PennMilliSliceDump();

	virtual void analyze(art::Event const & evt);

private:
	void beginJob();

	std::string raw_data_label_;
	std::string frag_type_;
	std::vector<int> verb_microslice_ids_;
	std::vector<int> verb_payload_ids_;
};


dune::PennMilliSliceDump::PennMilliSliceDump(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type")),
  verb_microslice_ids_(pset.get<std::vector<int>>("verbose_microslice_ids", std::vector<int>(1,0))),
  verb_payload_ids_   (pset.get<std::vector<int>>("verbose_payload_ids",    std::vector<int>(1,0)))
{
}

void dune::PennMilliSliceDump::beginJob()
{
}

dune::PennMilliSliceDump::~PennMilliSliceDump()
{
}

void dune::PennMilliSliceDump::analyze(art::Event const & evt)
{

  art::EventNumber_t eventNumber = evt.event();

  // ***********************
  // *** PennMilliSlice Fragments ***
  // ***********************

  ///> look for raw PennMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);

  if (raw.isValid()) {
    DAQLogger::LogInfo("PennMilliSliceDump") << "######################################################################" << std::endl;
    DAQLogger::LogInfo("PennMilliSliceDump") << std::endl;
    DAQLogger::LogInfo("PennMilliSliceDump") << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has " << raw->size()
	      << " fragment(s) of type " << frag_type_ << std::endl;

    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);

	///> Create a PennMilliSliceFragment from the generic artdaq fragment
	PennMilliSliceFragment msf(frag);

	///> Find the number of each type of payload frame found in the millislice
	dune::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
	n_frames = msf.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);

	///> Another way to find only the total number of payload frames in the millislice
	//dune::PennMilliSlice::Header::payload_count_t n_frames_again = msf.payloadCount();

	///> Find the total number of bytes in the millislice
	dune::PennMilliSlice::Header::millislice_size_t ms_size = msf.size();

	///> Find the millislice version number
	dune::PennMilliSlice::Header::version_t ms_version = msf.version();

	///> Find the millislice sequence ID
	dune::PennMilliSlice::Header::sequence_id_t ms_sequence_id = msf.sequenceID();

	///> Find the timestamp signifying the end of the millislice
	dune::PennMilliSlice::Header::timestamp_t ms_end_timestamp = msf.endTimestamp();

	///> Find the width of the millislice in NOvA clock ticks (excluding the overlap)
	dune::PennMilliSlice::Header::ticks_t ms_width_in_ticks = msf.widthTicks();

	///> Find the width of the millislice overlap period in NOvA clock ticks
	dune::PennMilliSlice::Header::ticks_t ms_overlap_in_ticks = msf.overlapTicks();

	DAQLogger::LogInfo("PennMilliSliceDump") << std::endl
		  << "PennMilliSlice fragment ID " << frag.fragmentID()
		  << " with version " << ms_version << " and sequence ID " << ms_sequence_id
		  << " consists of: " << ms_size << " bytes containing "
		  << std::endl
		  << " "
		  << n_frames            << " total frames ("
		  << n_frames_counter    << " counter + "
		  << n_frames_trigger    << " trigger + "
		  << n_frames_timestamp  << " timestamp + "
		  << n_frames - n_frames_counter - n_frames_trigger - n_frames_timestamp << " warning )"
		  << std::endl
		  << " with width " << ms_width_in_ticks << " ticks (excluding overlap of " << ms_overlap_in_ticks
		  << " ticks) and end timestamp " << ms_end_timestamp << std::endl
		  << std::endl << std::endl;

	///> Create variables to store the payload information
	dune::PennMicroSlice::Payload_Header::data_packet_type_t type;
	dune::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
	uint8_t* payload_data;
	size_t payload_size;

	///> Loop over payload frames
	for (uint32_t ip = 0; ip < n_frames; ip++) {

	  ///> Get a pointer to the N*32-bit payload data for payload ID ip, and also get the payload type, timestamp, and size
	  payload_data = msf.payload(ip, type, timestamp, payload_size);

	  bool verb_payload = ((std::find(verb_payload_ids_.begin(), verb_payload_ids_.end(), ip) != verb_payload_ids_.end())
			       || (verb_payload_ids_.size() == 1 && verb_payload_ids_.at(0) > 999999)) ? true : false;
	  if((payload_data != nullptr) && payload_size && verb_payload) {
	    DAQLogger::LogInfo("PennMilliSliceDump") << "Payload " << ip << " is type " << std::hex << (unsigned int)type << std::dec
		      << " with timestamp " << timestamp << " and contents ";
	    for(size_t ib = 0; ib < payload_size; ib++)
	      DAQLogger::LogInfo("PennMilliSliceDump") << std::bitset<8>(*(payload_data + ib)) << " ";
	    DAQLogger::LogInfo("PennMilliSliceDump") << std::endl;
	  }//payload_data && payload_size
	}//ip

      }//raw fragment loop
  }//raw.IsValid()?
  else {
    DAQLogger::LogInfo("PennMilliSliceDump") << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has zero"
	      << " PennMilliSlice fragments." << std::endl;
  }
  DAQLogger::LogInfo("PennMilliSliceDump") << std::endl;

}

DEFINE_ART_MODULE(dune::PennMilliSliceDump)
