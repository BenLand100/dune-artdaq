
#include "lbne-artdaq/Generators/Playback.hh"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <string>

#include <unistd.h>

lbne::Playback::Playback(fhicl::ParameterSet const & ps) :
  CommandableFragmentGenerator(ps),
  throttle_usecs_(ps.get<std::size_t>("throttle_usecs",100000)),
  driver_mode_(ps.get<bool>("driver_mode")),
  input_file_list_(ps.get<std::string>("input_file_list")),
  input_file_(std::make_unique<std::ifstream>(input_file_list_)),
  input_file_iter_(*input_file_),
  root_file_(nullptr),
  tree_(nullptr),
  branch_(nullptr)
{
  if (!input_file_->is_open()) {
    throw cet::exception("Playback") << "Unable to open \"" << input_file_list_ << 
      "\", does it exist?";
  }

  input_root_filenames_.assign(input_file_iter_, 
			       std::istream_iterator< std::string >());

  if (input_root_filenames_.size() == 0) {
    throw cet::exception("Playback") << "No files parsed from list in " << input_file_list_;
  }

  DAQLogger::LogInfo("Playback") << "\n" << input_root_filenames_.size() << " files listed, as follows: ";

  for (auto& filename : input_root_filenames_) {
    DAQLogger::LogInfo("Playback") << filename << std::endl;
  }

  // JCF, Jun-4-2016

  // In a nutshell, if running the full set of artdaq processes, I get
  // a segfault if I call setBranchFromFile in the
  // constructor. However, if running this fragment generator off of
  // driver, setBranchFromFile ONLY works in the
  // constructor. Phenomenon not yet understood.

  // Note that in driver mode, we can't currently see messagefacility
  // messages - thus the use of "cout" here

  if (driver_mode_) {
    std::cout << "In driver mode; will only look at first file listed in " << 
      input_file_list_ << std::endl;
    setBranchFromFile(input_root_filenames_.at(0));
  }

}


bool lbne::Playback::getNext_(artdaq::FragmentPtrs& outputFrags) {

  if (should_stop()) {
    return false;
  }

  static size_t file_index = 0;
  static size_t entry_number = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

  bool first_overall_event = file_index == 0 && entry_number == 0;

  bool turnover = first_overall_event || (branch_ && branch_->GetEntries() == entry_number);

  DAQLogger::LogDebug("Playback") << "file_index == " << file_index << ", entry_number == " <<
    entry_number << ", turnover == " << turnover << std::endl;

#pragma GCC diagnostic pop

  if ( driver_mode_ && turnover && !first_overall_event) {
    std::cout << "Finished processing first file in driver_mode; exiting" << std::endl;
    return false;
  }

  if (! driver_mode_ && turnover ) {
    
    if (!first_overall_event) {
      file_index++;
      entry_number = 0;
    }

    if (file_index == input_root_filenames_.size()) {
      DAQLogger::LogInfo("Playback") << "Finished processing all " << input_root_filenames_.size() << 
	" files listed in " << input_file_list_ << std::endl;
      return false;
    }

    setBranchFromFile(input_root_filenames_.at(file_index));
  } 

  branch_->GetEntry(entry_number);

  artdaq::Fragments* fragments = reinterpret_cast <artdaq::Fragments*>(branch_->GetAddress());

  for (auto& fragment : *fragments) {

    if (fragment.fragmentID() == fragment_id()) {

      std::unique_ptr<artdaq::Fragment> fragmentCopy = 
	std::make_unique<artdaq::Fragment>(fragment);

      if (driver_mode_) {
	std::cout << "frag copy has sequence ID " << fragmentCopy->sequenceID() << std::endl;
	std::cout << "frag copy has fragment ID " << fragmentCopy->fragmentID() << std::endl;
	std::cout << "frag copy has size " << fragmentCopy->sizeBytes() << std::endl;
	std::cout << "frag copy has type " << fragmentTypeToString(static_cast<FT>(fragment.type())) << std::endl;
      }

      outputFrags.emplace_back( std::move( fragmentCopy ) );
    }
  }

  entry_number++;

  if(metricMan_ != nullptr) {
    metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  }

  ev_counter_inc();

  usleep(throttle_usecs_);

  return true;
}

void lbne::Playback::setBranchFromFile(const std::string filename) {

  root_file_ = TFile::Open(filename.c_str());

  if (!root_file_ || !root_file_->IsOpen()) {
    throw cet::exception("Playback") << "Unable to open TFile " << filename;
  } 

  tree_ = static_cast<TTree*>( root_file_->Get("Events") );
  
  if (!tree_) {
    throw cet::exception("Playback") << "Unable to get tree from " << filename;
  }

  // Figure out which branch contains the fragment ID we want

  bool foundFragmentType = false;

  for (auto& fragtype : possible_fragment_types_) {

    if (foundFragmentType) {
      break;
    }

    const std::string tagargs = "daq:" + fragmentTypeToString(fragtype) + ":DAQ";
    const art::InputTag tag( tagargs );

    TBranch* branch = tree_->GetBranch(getBranchNameAsCStr<artdaq::Fragments>(tag));
    
    if (branch) {
      
      if (branch->GetEntries() > 0) {
	branch->GetEntry(0);

	artdaq::Fragments* fragments = reinterpret_cast <artdaq::Fragments *> (
									       branch->GetAddress());

	for (auto& frag : *fragments) {
	  
	  if ( frag.fragmentID() == fragment_id()) {
	    foundFragmentType = true;
	    branch_ = branch;
	    break;
	  }
	}
      }
    } else {
      throw cet::exception("Playback") << "Didn't find branch corresponding to tag \"" << tagargs << "\"";
    }
  }

  if (!foundFragmentType) {
    throw cet::exception("Playback") << "Unable to find branch in \"" << filename << "\" containing "
				     << " fragment(s) with requested fragment ID #" << fragment_id();
  }
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::Playback) 
