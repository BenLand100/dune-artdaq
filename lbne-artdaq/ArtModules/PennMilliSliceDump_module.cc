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
#include "art/Framework/Services/Optional/TFileService.h"

#include "art/Utilities/Exception.h"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"
#include "lbne-artdaq/Generators/pennBoard/PennCompileOptions.hh"

#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>
#include <bitset>

namespace lbne {
class PennMilliSliceDump;
}

class lbne::PennMilliSliceDump : public art::EDAnalyzer {
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


lbne::PennMilliSliceDump::PennMilliSliceDump(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type")),
  verb_microslice_ids_(pset.get<std::vector<int>>("verbose_microslice_ids", std::vector<int>(1,0))),
  verb_payload_ids_   (pset.get<std::vector<int>>("verbose_payload_ids",    std::vector<int>(1,0)))
{
}

void lbne::PennMilliSliceDump::beginJob()
{
	art::ServiceHandle<art::TFileService> tfs;
}

lbne::PennMilliSliceDump::~PennMilliSliceDump()
{
}

void lbne::PennMilliSliceDump::analyze(art::Event const & evt)
{
  art::EventNumber_t eventNumber = evt.event();

  // ***********************
  // *** PennMilliSlice Fragments ***
  // ***********************

  ///> look for raw PennMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, frag_type_, raw);

  if (raw.isValid()) {
    std::cout << "######################################################################" << std::endl;
    std::cout << std::endl;
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has " << raw->size()
	      << " fragment(s) of type " << frag_type_ << std::endl;

    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);

	///> Create a PennMilliSliceFragment from the generic artdaq fragment
	PennMilliSliceFragment msf(frag);

	///> Find the number of each type of payload word found in the millislice
	lbne::PennMilliSlice::Header::payload_count_t n_words, n_words_counter, n_words_trigger, n_words_timestamp;
	n_words = msf.payloadCount(n_words_counter, n_words_trigger, n_words_timestamp);

	///> Another way to find only the total number of payload words in the millislice
	//lbne::PennMilliSlice::Header::payload_count_t n_words_again = msf.payloadCount();

	///> Find the total number of bytes in the millislice
	lbne::PennMilliSlice::Header::millislice_size_t ms_size = msf.size();

	///> Find the millislice version number
	lbne::PennMilliSlice::Header::version_t ms_version = msf.version();

	///> Find the millislice sequence ID
	lbne::PennMilliSlice::Header::sequence_id_t ms_sequence_id = msf.sequenceID();

	///> Find the timestamp signifying the end of the millislice
	lbne::PennMilliSlice::Header::timestamp_t ms_end_timestamp = msf.endTimestamp();

	///> Find the width of the millislice in NOvA clock ticks (excluding the overlap)
	lbne::PennMilliSlice::Header::ticks_t ms_width_in_ticks = msf.widthTicks();

	///> Find the width of the millislice overlap period in NOvA clock ticks
	lbne::PennMilliSlice::Header::ticks_t ms_overlap_in_ticks = msf.overlapTicks();

	std::cout << std::endl
		  << "PennMilliSlice fragment " << frag.fragmentID()
		  << " with version " << ms_version << " and sequence ID " << ms_sequence_id
		  << " consists of: " << ms_size << " bytes containing "
#ifndef REBLOCK_PENN_USLICE
		  << msf.microSliceCount() << " microslices and"
#endif
		  << std::endl
		  << " "
		  << n_words            << " total words ("
		  << n_words_counter    << " counter + "
		  << n_words_trigger    << " trigger + "
		  << n_words_timestamp  << " timestamp)"
		  << std::endl
		  << " with width " << ms_width_in_ticks << " ticks (excluding overlap of " << ms_overlap_in_ticks
		  << " ticks) and end timestamp " << ms_end_timestamp
		  << std::endl << std::endl;

#ifdef REBLOCK_PENN_USLICE
	///> Create variables to store the payload information
	lbne::PennMicroSlice::Payload_Header::data_packet_type_t type;
	lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
	uint8_t* payload_data;
	size_t payload_size;

	///> Loop over payload words
	for (uint32_t ip = 0; ip < n_words; ip++) {

	  ///> Get a pointer to the N*32-bit payload data for payload ID ip, and also get the payload type, timestamp, and size
	  payload_data = msf.payload(ip, type, timestamp, payload_size);

	  bool verb_payload = ((std::find(verb_payload_ids_.begin(), verb_payload_ids_.end(), ip) != verb_payload_ids_.end())
			       || (verb_payload_ids_.size() == 1 && verb_payload_ids_.at(0) > 999999)) ? true : false;
	  if((payload_data != nullptr) && payload_size && verb_payload) {
	    std::cout << "Payload " << ip << " is type " << std::hex << (unsigned int)type << std::dec
		      << " with timestamp " << timestamp << " and contents ";
	    for(size_t ib = 0; ib < payload_size; ib++)
	      std::cout << std::bitset<8>(*(payload_data + ib)) << " ";
	    std::cout << std::endl;
	  }//payload_data && payload_size
	}//ip
#else
	for (uint32_t i_ms = 0; i_ms < msf.microSliceCount(); ++i_ms)
	  {
	    bool verb_microslice = (std::find(verb_microslice_ids_.begin(), verb_microslice_ids_.end(), i_ms) != verb_microslice_ids_.end()) ? true : false;

	    std::unique_ptr<const PennMicroSlice> microslice = msf.microSlice(i_ms);

	    if (!microslice) {
	      throw cet::exception("Error in PennMilliSliceDump module: unable to find requested microslice");
	    }

	    if(verb_microslice) {
	      std::cout << "PennMilliSlice fragment " << frag.fragmentID()
			<< ", microslice " << i_ms
			<< " has sequence ID " << (unsigned int)microslice->sequence_id()
			<< ", version 0x" << std::hex << (unsigned int)microslice->format_version() << std::dec
			<< ", size " << microslice->size()
			<< " bytes" << std::endl;

	      //Now get the number of data words
	      uint32_t n_data_words(0);
	      uint32_t n_data_words_counter(0);
	      uint32_t n_data_words_trigger(0);
	      uint32_t n_data_words_timestamp(0);
	      n_data_words = microslice->sampleCount(n_data_words_counter, n_data_words_trigger, n_data_words_timestamp, false);
	      std::cout << "Microslice contains " << n_data_words << " data words ("
			<< n_data_words_counter   << " counter + "
			<< n_data_words_trigger   << " trigger + "
			<< n_data_words_timestamp << " timestamp)"
			<< std::endl;

	      for(uint32_t ip = 0; ip < n_data_words; ip++) {
		bool verb_payload = (std::find(verb_payload_ids_.begin(), verb_payload_ids_.end(), ip) != verb_payload_ids_.end()) ? true : false;
		if(verb_payload) {
		  lbne::PennMicroSlice::Payload_Header::data_packet_type_t     data_packet_type;
		  lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t short_nova_timestamp;
		  size_t   payload_size;
		  uint8_t* payload = microslice->get_payload(uint32_t(ip), data_packet_type, short_nova_timestamp, payload_size, false);
		  if(payload != nullptr) {
		    std::cout << "Payload " << ip << " is type " << std::hex << (unsigned int)data_packet_type << std::dec
			      << " with timestamp " << short_nova_timestamp << " and contents ";
		    for(size_t ib = 0; ib < payload_size; ib++)
		      std::cout << std::bitset<8>(*(payload+ib)) << " ";
		    std::cout << std::endl;
		  }//payload
		}//verb_payload
	      }//n_data_words loop
	    }//verb_microslice
	  }//microslice loop
#endif
      }//raw fragment loop
  }//raw.IsValid()?
  else {
    std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
	      << ", event " << eventNumber << " has zero"
	      << " PennMilliSlice fragments." << std::endl;
  }
  std::cout << std::endl;

}

DEFINE_ART_MODULE(lbne::PennMilliSliceDump)
