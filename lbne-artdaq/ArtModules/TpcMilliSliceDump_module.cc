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
#include "art/Framework/Services/Optional/TFileService.h"

#include "art/Utilities/Exception.h"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "artdaq-core/Data/Fragments.hh"

#include "TH1.h"

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
	void beginJob();

	std::string raw_data_label_;
	std::string frag_type_;
	std::vector<int> verb_microslice_ids_;
	std::vector<int> verb_nanoslice_ids_;
	TH1D * adc_values_;
};


lbne::TpcMilliSliceDump::TpcMilliSliceDump(fhicl::ParameterSet const & pset)
: EDAnalyzer(pset),
  raw_data_label_(pset.get<std::string>("raw_data_label")),
  frag_type_(pset.get<std::string>("frag_type")),
  verb_microslice_ids_(pset.get<std::vector<int>>("verbose_microslice_ids", std::vector<int>(1,0))),
  verb_nanoslice_ids_ (pset.get<std::vector<int>>("verbose_nanoslice_ids",  std::vector<int>(1,0))),
  adc_values_(nullptr)
{
}

void lbne::TpcMilliSliceDump::beginJob()
{
	art::ServiceHandle<art::TFileService> tfs;
	adc_values_ = tfs->make<TH1D>("adc_values","adc_values",4096,-0.5,4095.5);
}

lbne::TpcMilliSliceDump::~TpcMilliSliceDump()
{
}

void lbne::TpcMilliSliceDump::analyze(art::Event const & evt)
{
	//counter of total number of ALL adc values in the event
	uint32_t n_adc_counter(0);
	//cumulative total of ALL adc values in the event
	uint64_t adc_cumulative(0);

	art::EventNumber_t eventNumber = evt.event();

	// ***********************
	// *** TpcMilliSlice Fragments ***
	// ***********************

	///> look for raw TpcMilliSlice data
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

			///> Create a TpcMilliSliceFragment from the generic artdaq fragment
			TpcMilliSliceFragment msf(frag);

			///> get the total size of the millislice (data+header, in bytes)
			lbne::TpcMilliSlice::Header::millislice_size_t msf_size = msf.size();

			///> get the number of MicroSlices in the MilliSlice
			lbne::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();
			
			/* methods not yet implemented
			///> get the millislice version
			lbne::TpcMilliSlice::Header::version_t msf_version = msf.version();

			///> get the millislice fixed pattern
			lbne::TpcMilliSlice::Header::pattern_t msf_fixed_pattern = msf.fixed_pattern();
			*/

			std::cout << std::endl
				  << "TpcMilliSlice fragment " << frag.fragmentID() << " consists of: " << msf_size << " bytes containing "
				  << msf_us_count << " microslices" << std::endl
				  //<< "Version " << msf_version << " pattern " << msf_fixed_pattern << std::endl
				  << std::endl;

			///> loop over the number of microslices
			for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us)
			{
				bool verb_microslice = (std::find(verb_microslice_ids_.begin(), verb_microslice_ids_.end(), i_us) != verb_microslice_ids_.end()) ? true : false;

				///> get the 'i_us-th' microslice
				std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);

				if (!microslice) {
					throw cet::exception("Error in TpcMilliSliceDump module: unable to find requested microslice");
				}

				///> get the microslice sequence ID
				lbne::TpcMicroSlice::Header::sequence_id_t us_sequence_id = microslice->sequence_id();

				///> get the microslice total size (header+data, in bytes)
				lbne::TpcMicroSlice::Header::microslice_size_t us_size = microslice->size();

				///> get the number of NanoSlices in this MicroSlice
				lbne::TpcMicroSlice::Header::nanoslice_count_t us_nanocount = microslice->nanoSliceCount();

				///> get the microslice type ID
				lbne::TpcMicroSlice::Header::type_id_t us_type_id = microslice->type_id();

				uint8_t us_run_mode = (us_type_id >> 16) & 0xF;

				///> get the software message
				lbne::TpcMicroSlice::Header::softmsg_t us_software_message = microslice->software_message();

				///> get the hardware (firmware) message
				lbne::TpcMicroSlice::Header::firmmsg_t us_firmware_message = microslice->firmware_message();

				if(verb_microslice) {
					std::cout << "TpcMilliSlice fragment " << frag.fragmentID()
			    		<< ", microslice " << i_us
			    		<< " has sequence ID " << us_sequence_id
			    		<< " size " << us_size
			    		<< " and consists of: "<< us_nanocount << " nanoslices" << std::endl;
					std::cout << " frame size header    : 0x"
						  << std::hex << std::setw(8) << std::setfill('0') << us_size <<std::dec << std::endl;
					std::cout << " sequence ID header    : 0x"
						  << std::hex << std::setw(8) << std::setfill('0') << us_sequence_id <<std::dec << std::endl;
					std::cout << " type ID header    : 0x"
						  << std::hex << std::setw(8) << std::setfill('0') << us_type_id <<std::dec << std::endl
						  << "\t\t4 bit flags [error,software trigger,external trigger,dropped frame]:" << std::bitset<4>((us_type_id >> 28) & 0xF) << std::endl
						  << "\t\tRun mode ID: " << (uint16_t)us_run_mode << std::endl
						  << "\t\tRCE software version: 0x" << std::hex << std::setw(4) << std::setfill('0') << (us_type_id & 0xFFFF) << std::endl;
					std::cout << " software message    : 0x"
						  << std::hex << std::setw(16) << std::setfill('0') << us_software_message <<std::dec << std::endl 
						  << "\t\t";
					if(us_run_mode == 0x0)
					  std::cout << "Run mode Idle. NOvA timestamp from first nanoslice: " << us_software_message << std::endl;
					else if(us_run_mode == 0x1)
					  std::cout << "Run mode Scope. Selected channel: " << us_software_message << std::endl;
					else if(us_run_mode == 0x2)
					  std::cout << "Run mode Burst. Software message 0: " << us_software_message << std::endl;
					else if(us_run_mode == 0x3) {
					  std::cout << "Run mode Trigger. " << std::endl
						    << "\t\t";
					  if(((us_type_id >> 28) & 0x6) == 0x4)
					    std::cout << "255: " << ((us_software_message >> 56) & 0xFF)
						      << " NOvA timestamp of first nanoslice in microslice: " << (us_software_message & 0xFFFFFFFFFFFFFF) << std::endl;
					  else
					    std::cout << "Trigger counter: " << ((us_software_message >> 56) & 0xFF)
						      << " NOvA timestamp of external trigger: " << (us_software_message & 0xFFFFFFFFFFFFFF) << std::endl;
					}
					else
					  std::cout << "Unknown run mode" << std::endl;
					std::cout << " firmware message    : 0x"
						  << std::hex << std::setw(16) << std::setfill('0') << us_firmware_message <<std::dec << std::endl;								
				}
				
				if (microslice->nanoSliceCount() == 0) continue;

				// First pass through nanoslices listed for verbose output. This is faster than iterating through many nanoslices
				// and searching for the index in the verbose list

				for (std::vector<int>::iterator i_nano = verb_nanoslice_ids_.begin(); i_nano != verb_nanoslice_ids_.end(); i_nano++)
				{
					bool verb_nanoslice_header = verb_microslice;
					if (verb_nanoslice_header && (std::find(verb_nanoslice_ids_.begin(), verb_nanoslice_ids_.end(), *i_nano) == verb_nanoslice_ids_.end()))
					{
						verb_nanoslice_header = false;
					}

					///> get the 'i_nano-th' NanoSlice
					std::unique_ptr<const TpcNanoSlice> nanoslice = microslice->nanoSlice(*i_nano);
					if (! nanoslice )
					{
						throw cet::exception("Error in TpcMilliSliceDump module: Unable to locate requested nanoslice");
					}
					if (verb_nanoslice_header)
					{

					        ///> get the size of the nanoslice (header+data, in bytes)
  					        lbne::TpcNanoSlice::nanoslice_size_t ns_size = nanoslice->size();

						std::cout << "  NanoSlice " << *i_nano << " size " << ns_size << " bytes" << std::endl << std::endl;

						///> get a pointer to the first 64-bit word of the nanoslice (i.e. the first header word)
						const uint64_t* nanoslice_raw = nanoslice->raw();

						std::cout << "    Raw Header : 0x"
								<< std::hex << std::setw(16) << std::setfill('0') << nanoslice_raw[0] <<std::dec << std::endl;

						std::cout << "    Raw Data Payload:" << std::endl << "    ";

						///> get the number of nanoslice data words
						uint16_t num_nanoslice_words = (nanoslice->size() - sizeof(lbne::TpcNanoSlice::Header))/sizeof(uint64_t);

						///> find out how many uint64_t's the header is
						size_t word_offset = sizeof(lbne::TpcNanoSlice::Header)/sizeof(uint64_t);

						///> loop over all 16-bit ADC values
						for (int i_adc_word = 0; i_adc_word < num_nanoslice_words; i_adc_word++)
						{
							std::cout << "0x" << std::hex << std::setfill('0') << std::setw(16) << nanoslice_raw[word_offset+i_adc_word] << std::dec << " ";
							if ((i_adc_word % 4) == 3) std::cout << std::endl << "    ";
						}
						std::cout << std::endl;
						
						///> get the NOvA timestamp
						lbne::TpcNanoSlice::Header::nova_timestamp_t ns_timestamp = nanoslice->nova_timestamp();

						//std::cout << "    Decoded Header:" << std::endl;
						std::cout << "    NOvA timestamp : 0x" << std::hex << std::setw(14) << std::setfill('0')
							  << ns_timestamp << std::dec
							  << "\t" << ns_timestamp
							  << "\t" << std::bitset<64>(ns_timestamp)
							  << std::endl;

						///> get the number of channels contained in the nanoslice
						uint32_t ns_nchan = nanoslice->getNChannels();
						///> an alternative way to get the number of samples in the nanoslice
						//lbne::TpcNanoSlice::sample_count_t ns_chan_again = nanoslice->sampleCount();

						std::cout << "    Decoded ADC Values:" << std::endl << "    ";
						///> loop over all 16-bit ADC values
						for (uint32_t i_samp = 0; i_samp < ns_nchan; i_samp++)
						{
							uint16_t val = std::numeric_limits<uint16_t>::max();

							///> get the 'i_samp-th' ADC value from the NanoSlice
							nanoslice->sampleValue(i_samp, val);

							std::cout << std::setfill(' ') << std::setw(4) << i_samp << " : " << std::setw(4) << val << " ";
							if ((i_samp % 8) == 7) std::cout << std::endl << "    ";

						} // i_samp
						std::cout << std::endl;
					}
				} // i_nano

				std::cout << std::endl;

				// Make a second pass through all nanoslices to build some statistics and fill histograms
				///> Loop over the NanoSlice's in this MicroSlice
				for (uint32_t i_nano = 0; i_nano < microslice->nanoSliceCount(); i_nano++)
				{
				        ///> get the 'i_nano-th' NanoSlice
				        std::unique_ptr<const TpcNanoSlice> nanoslice = microslice->nanoSlice(i_nano);

					if (!nanoslice)
					{
						throw cet::exception("Error in TpcMilliSliceDump module: Unable to find requested nanoslice");
					}

					///> loop over all 16-bit ADC values
					for (uint32_t i_samp = 0; i_samp < nanoslice->sampleCount(); i_samp++)
					{
						uint16_t val = std::numeric_limits<uint16_t>::max();

						///> get the 'i_samp-th' ADC value from the NanoSlice
						bool success = nanoslice->sampleValue(i_samp, val);

						if (!success)
						{
							throw cet::exception("Error in TpcMilliSliceDump module: Unable to find the requested sample value in nanoslice");
						}
						n_adc_counter++;
						adc_cumulative += (uint64_t)val;
						adc_values_->Fill(val);
					} // i_samp
				} // i_nano
			} // i_us

		} //idx

		std::cout << std::endl
				<< "Event ADC average is (from counter):   " << (double)adc_cumulative/(double)n_adc_counter
				<< std::endl
				<< "Event ADC average is (from histogram): " << adc_values_->GetMean()
				<< std::endl;

	}//raw.IsValid()?
	else {
		std::cout << "Run " << evt.run() << ", subrun " << evt.subRun()
				  << ", event " << eventNumber << " has zero"
				  << " TpcMilliSlice fragments." << std::endl;
	}
	std::cout << std::endl;

}

DEFINE_ART_MODULE(lbne::TpcMilliSliceDump)
