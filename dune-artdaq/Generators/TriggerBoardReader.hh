#ifndef dune_artdaq_Generators_TriggerBoardReader_hh
#define dune_artdaq_Generators_TriggerBoardReader_hh

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Generators/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include "TriggerBoard/CTB_Controller.hh"
#include "TriggerBoard/CTB_Receiver.hh"

#include <random>
#include <vector>
#include <atomic>


namespace dune {    

  class TriggerBoardReader : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TriggerBoardReader(fhicl::ParameterSet const & ps);
    ~TriggerBoardReader();

  private:

    // The "getNext_" function is used to implement user-specific
    // functionality; it's a mandatory override of the pure virtual
    // getNext_ function declared in CommandableFragmentGenerator

    bool getNext_(artdaq::FragmentPtrs & output) override;

    // The start, stop and stopNoMutex methods are declared pure
    // virtual in CommandableFragmentGenerator and therefore MUST be
    // overridden; note that stopNoMutex() doesn't do anything here

    void start() override;
    void stop() override;
    void stopNoMutex() override {}

    // utilities
    artdaq::FragmentPtr CreateFragment() ;

    std::unique_ptr<CTB_Controller> _run_controller ; 
    std::unique_ptr<CTB_Receiver>   _receiver ; 

    unsigned int _group_size ;
    unsigned int _max_words_per_frag ;    

    unsigned int _max_frags_per_call ;

    bool _has_last_TS = false ;
    artdaq::Fragment::timestamp_t _last_timestamp = artdaq::Fragment::InvalidTimestamp ;

    FragmentType fragment_type_;
    bool throw_exception_;
  };
}

#endif /* dune_artdaq_Generators_TriggerBoardReader_hh */
