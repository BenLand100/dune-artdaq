
#include "dune-artdaq/Generators/ToyHardwareInterface/ToyHardwareInterface.hh"
#include "dune-raw-data/Overlays/ToyFragment.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "fhiclcpp/ParameterSet.h"
#include "cetlib/exception.h"

#include <random>
#include <unistd.h>
#include <iostream>

// JCF, Mar-17-2016

// ToyHardwareInterface is meant to mimic a vendor-provided hardware
// API, usable within the the ToySimulator fragment generator. For
// purposes of realism, it's a C++03-style API, as opposed to, say, one
// based in C++11 capable of taking advantage of smart pointers, etc.

ToyHardwareInterface::ToyHardwareInterface(fhicl::ParameterSet const & ps) :
  //  taking_data_(false),
  taking_data_(true), // See Aug-14-2016 comment, below
  nADCcounts_(ps.get<size_t>("nADCcounts", 40)), 
  maxADCcounts_(ps.get<size_t>("maxADCcounts", 50000000)),
  change_after_N_seconds_(ps.get<size_t>("change_after_N_seconds", 
					 std::numeric_limits<size_t>::max())),
  nADCcounts_after_N_seconds_(ps.get<int>("nADCcounts_after_N_seconds",
					  nADCcounts_)),
  fragment_type_(dune::toFragmentType(ps.get<std::string>("fragment_type"))), 
  maxADCvalue_(pow(2, NumADCBits() ) - 1), // MUST be after "fragment_type"
  throttle_usecs_(ps.get<size_t>("throttle_usecs", 100000)),
  distribution_type_(static_cast<DistributionType>(ps.get<int>("distribution_type"))),
  engine_(ps.get<int64_t>("random_seed", 314159)),
  uniform_distn_(new std::uniform_int_distribution<data_t>(0, maxADCvalue_)),
  gaussian_distn_(new std::normal_distribution<double>( 0.5*maxADCvalue_, 0.1*maxADCvalue_)),
  start_time_(std::numeric_limits<decltype(std::chrono::high_resolution_clock::now())>::max())
{

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

  // JCF, Aug-14-2016 

  // The logic of checking that nADCcounts_after_N_seconds_ >= 0,
  // below, is because signed vs. unsigned comparison won't do what
  // you want it to do if nADCcounts_after_N_seconds_ is negative

  if (nADCcounts_ > maxADCcounts_ ||
      (nADCcounts_after_N_seconds_ >= 0 && nADCcounts_after_N_seconds_ > maxADCcounts_)) {
    throw cet::exception("HardwareInterface") << "Either (or both) of \"nADCcounts\" and \"nADCcounts_after_N_seconds\"" <<
      " is larger than the \"maxADCcounts\" setting (currently at " << maxADCcounts_ << ")";
  }

  if (nADCcounts_after_N_seconds_ != nADCcounts_ && 
      change_after_N_seconds_ == std::numeric_limits<size_t>::max()) {
    throw cet::exception("HardwareInterface") << "If \"nADCcounts_after_N_seconds\""
					      << " is set, then \"change_after_N_seconds\" should be set as well";
#pragma GCC diagnostic pop
      
  }
}

// JCF, Mar-18-2017

// "StartDatataking" is meant to mimic actions one would take when
// telling the hardware to start sending data - the uploading of
// values to registers, etc.

void ToyHardwareInterface::StartDatataking() {

  // JCF, Aug-14-2017

  // Until we base dune-artdaq off of a newer version of artdaq
  // s.t. its artdaqDriver program can issue start / stop transitions,
  // require that we ALWAYS be allowed to take data

  assert(taking_data_);

  taking_data_ = true;
  start_time_ = std::chrono::high_resolution_clock::now();
}

void ToyHardwareInterface::StopDatataking() {
  //  taking_data_ = false;
  start_time_ = std::numeric_limits<decltype(std::chrono::high_resolution_clock::now())>::max();
}


void ToyHardwareInterface::FillBuffer(char* buffer, size_t* bytes_read) {

  if (taking_data_) {

    usleep( throttle_usecs_ );

    auto elapsed_secs_since_datataking_start = 
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now()
						       - start_time_).count();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
    if (elapsed_secs_since_datataking_start < change_after_N_seconds_) {
#pragma GCC diagnostic pop

      *bytes_read = sizeof(dune::ToyFragment::Header) + nADCcounts_ * sizeof(data_t);
    } else {
      if (nADCcounts_after_N_seconds_ >= 0) {
	*bytes_read = sizeof(dune::ToyFragment::Header) + nADCcounts_after_N_seconds_ * sizeof(data_t);
      } else {
	// Pretend the hardware hangs
	while (true) {
	}
      }
    }
      
    // Make the fake data, starting with the header

    // Can't handle a fragment whose size isn't evenly divisible by
    // the dune::ToyFragment::Header::data_t type size in bytes

    assert( *bytes_read % sizeof(dune::ToyFragment::Header::data_t) == 0 );

    dune::ToyFragment::Header* header = reinterpret_cast<dune::ToyFragment::Header*>(buffer);

    header->event_size = *bytes_read / sizeof(dune::ToyFragment::Header::data_t) ;
    header->run_number = 99;

    // Generate nADCcounts ADC values ranging from 0 to max based on
    // the desired distribution

    std::function<data_t()> generator;
    data_t gen_value = 0;

    switch (distribution_type_) {
    case DistributionType::uniform:
      generator = [&]() {
	return static_cast<data_t>
	((*uniform_distn_)( engine_ ));
      };
      break;

    case DistributionType::gaussian:
      generator = [&]() {

	do {
	  gen_value = static_cast<data_t>( std::round( (*gaussian_distn_)( engine_ ) ) );
	} 
	while(gen_value > maxADCvalue_);                                                                    
	return gen_value;
      };
      break;

    case DistributionType::monotonic:
      {
	generator = [&]() {
	  gen_value++;
	  return gen_value > maxADCvalue_ ? 9 : gen_value;
	};
      }
      break;

    default:
      throw cet::exception("HardwareInterface") <<
	"Unknown distribution type specified";
    }

    std::generate_n(reinterpret_cast<data_t*>( reinterpret_cast<dune::ToyFragment::Header*>(buffer) + 1 ), 
		    nADCcounts_,
		    generator
		    );

  } else {
    throw cet::exception("ToyHardwareInterface") <<
      "Attempt to call FillBuffer when not sending data";
  }
}

void ToyHardwareInterface::AllocateReadoutBuffer(char** buffer) {
  
  *buffer = reinterpret_cast<char*>( new uint8_t[ sizeof(dune::ToyFragment::Header) + maxADCcounts_*sizeof(data_t) ] );
}

void ToyHardwareInterface::FreeReadoutBuffer(char* buffer) {
  delete [] buffer;
}

// Pretend that the "BoardType" is some vendor-defined integer which
// differs from the fragment_type_ we want to use as developers (and
// which must be between 1 and 224, inclusive) so add an offset

int ToyHardwareInterface::BoardType() const {
  return static_cast<int>(fragment_type_) + 1000;
}

int ToyHardwareInterface::NumADCBits() const {

  switch (fragment_type_) {
  case dune::FragmentType::TOY1:
    return 12;
    break;
  case dune::FragmentType::TOY2:
    return 14;
    break;
  default:
    throw cet::exception("ToyHardwareInterface")
      << "Unknown board type "
      << fragment_type_
      << " ("
      << dune::fragmentTypeToString(fragment_type_)
      << ").\n";
  };

}

int ToyHardwareInterface::SerialNumber() const {
  return 999;
}

