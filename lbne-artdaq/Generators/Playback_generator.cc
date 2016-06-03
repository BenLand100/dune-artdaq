
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
  fragment_type_(static_cast<decltype(fragment_type_)>( artdaq::Fragment::InvalidFragmentType )),
  file_(nullptr),
  tree_(nullptr),
  branch_(nullptr)
{

  //  std::string filename = "/home/jcfree/scratch/developarea_lbne-artdaq_playback/lbne_r000101_sr01_20160603T161541.root";
  std::string filename = "/home/jcfree/scratch/input_files/lbne_r018798_sr01_20160601T211818.root";

  file_ = TFile::Open(filename.c_str());

  if (!file_->IsOpen()) {
    throw cet::exception("Playback") << "Unable to open TFile";
  }

  std::cout << "Opened up " << filename << std::endl;

  tree_ = static_cast<TTree*>( file_->Get("Events") );
  
  if (tree_ == nullptr) {
    throw cet::exception("Playback") << "Unable to get tree";
  }

  std::cout << "Obtained tree with " << tree_->GetEntries() << " entries" << std::endl;


  // Figure out which branch contains the fragment ID we want

  bool foundFragmentType = false;

  for (auto& fragtype : possible_fragment_types_) {

    if (foundFragmentType) {
      break;
    }

    const std::string tagargs = "daq:" + fragmentTypeToString(fragtype) + ":DAQ";
    const art::InputTag tag( tagargs );

    TBranch* branch = tree_->GetBranch(getBranchNameAsCStr<artdaq::Fragments>(tag));
    
    if (branch != nullptr) {
      //      std::cout << "Found branch corresponding to tag \"" << tagargs << "\"" << 
      //	" with " << branch->GetEntries() << " entries" << std::endl;
      
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
      throw cet::exception("Playback") << "Didn't find branch corresponding to tag \"" << tagargs << "\"" << std::endl;
    }
  }

  if (!foundFragmentType) {
    throw cet::exception("Playback") << "Unable to find branch in \"" << filename << "\" containing "
				     << " fragment(s) with requested fragment ID #" << fragment_id();
  }

}


bool lbne::Playback::getNext_(artdaq::FragmentPtrs& outputFrags) {

  static size_t entry_number = 0;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
  if (should_stop() || entry_number == branch_->GetEntries()-1) {
    return false;
  }
#pragma GCC diagnostic pop

  usleep(1000000);

  // if (entry_number == 0) {
  //   std::cout << "Branch address is " << static_cast<void*>( branch_ ) << std::endl;
  //   std::cout << ", branch has " << branch_->GetEntries() << " entries" << std::endl;
  // }

  branch_->GetEntry(entry_number);

  artdaq::Fragments* fragments = reinterpret_cast <artdaq::Fragments*>(branch_->GetAddress());

  for (auto& fragment : *fragments) {

    if (fragment.fragmentID() == fragment_id()) {

      std::unique_ptr<artdaq::Fragment> fragmentCopy = 
	std::make_unique<artdaq::Fragment>(fragment);

      std::cout << "frag copy has sequence ID " << fragmentCopy->sequenceID() << std::endl;
      std::cout << "frag copy has fragment ID " << fragmentCopy->fragmentID() << std::endl;
      std::cout << "frag copy has size " << fragmentCopy->sizeBytes() << std::endl;
      std::cout << "frag copy has type " << fragmentTypeToString(static_cast<FT>(fragment.type())) << std::endl;

      outputFrags.emplace_back( std::move( fragmentCopy ) );
    }
  }

  entry_number++;

  // artdaq::Fragment* myfrag = new artdaq::Fragment;
  // //  artdaq::Fragments* myfrags = new artdaq::Fragments;

  // branch_toy1_->SetAddress(&myfrag);
  // branch_toy1_->GetEntry(1);


  if(metricMan_ != nullptr) {
    metricMan_->sendMetric("Fragments Sent",ev_counter(), "Events", 3);
  }

  ev_counter_inc();

  return true;
}

// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::Playback) 
