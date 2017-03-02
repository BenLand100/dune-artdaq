
#include "dune-artdaq/Generators/Playback.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include "artdaq/Application/GeneratorMacros.hh"
#include "artdaq-core/Utilities/SimpleLookupPolicy.h"

#include "gallery/Handle.h"
#include "canvas/Utilities/Exception.h"
#include "canvas/Utilities/InputTag.h"
#include "canvas/Persistency/Provenance/EventID.h"
#include "canvas/Persistency/Provenance/RunID.h"
#include "canvas/Persistency/Provenance/SubRunID.h"
#include "canvas/Persistency/Common/FindMany.h"

#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <string>

#include <unistd.h>

dune::Playback::Playback(fhicl::ParameterSet const & ps) :
  CommandableFragmentGenerator(ps),
  throttle_usecs_(ps.get<std::size_t>("throttle_usecs",100000)),
  driver_mode_(ps.get<bool>("driver_mode", false)),
  input_file_list_(ps.get<std::string>("input_file_list")),
  force_sequential_(ps.get<bool>("force_sequential")),
  fragment_type_(ps.get<std::string>("fragment_type")),
  input_file_(std::make_unique<std::ifstream>(input_file_list_)),
  input_file_iter_(*input_file_)
{
  assert(input_file_);

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

  events_ = std::make_unique<gallery::Event>( input_root_filenames_ );
}


bool dune::Playback::getNext_(artdaq::FragmentPtrs& outputFrags) {

  assert(events_);
  
  if (should_stop() ) {
    return false;
  }

  if (events_->atEnd()) {
    DAQLogger::LogInfo("Playback") << "Finished processing all " << input_root_filenames_.size() << 
      " files listed in " << input_file_list_ << "; you can now issue the stop transition to the DAQ";
    return false;
  }

  static artdaq::Fragment::sequence_id_t entry_number = 0;
  bool foundFragment = false;

  std::string tag = "daq:" + fragment_type_ + ":DAQ";

  const auto& fragments = *( events_->getValidHandle<artdaq::Fragments>(tag) );

  for (auto& fragment : fragments) {

    if (fragment.fragmentID() == fragment_id()) {

      foundFragment = true;

      std::unique_ptr<artdaq::Fragment> fragmentCopy = 
	std::make_unique<artdaq::Fragment>(fragment);

      if (force_sequential_) {
	fragmentCopy->setSequenceID(entry_number+1);
      }

      if (driver_mode_) {
	std::cout << "frag copy has sequence ID " << fragmentCopy->sequenceID() << std::endl;
	std::cout << "frag copy has fragment ID " << fragmentCopy->fragmentID() << std::endl;
	std::cout << "frag copy has size " << fragmentCopy->sizeBytes() << std::endl;
	std::cout << "frag copy has type " << fragmentTypeToString(static_cast<FT>(fragment.type())) << std::endl;
      }

      outputFrags.emplace_back( std::move( fragmentCopy ) );
    }
  }

  if (!foundFragment) {
    DAQLogger::LogWarning("Playback") << "Unable to find fragment of type " << 
      fragment_type_ << " with fragment ID " << fragment_id() << 
      " for the " << entry_number << "-th received event";
  }

  entry_number++;

  if(metricMan_ != nullptr) {
    metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  }

  ev_counter_inc();

  usleep(throttle_usecs_);

  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::Playback) 
