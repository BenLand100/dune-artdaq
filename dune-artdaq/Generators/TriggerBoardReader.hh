#ifndef dune_artdaq_Generators_TriggerBoardReader_hh
#define dune_artdaq_Generators_TriggerBoardReader_hh

// Some C++ conventions used:

// -Append a "_" to every private member function and variable

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh" 
#include "artdaq/Application/CommandableFragmentGenerator.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"

#include <boost/lockfree/spsc_queue.hpp>

#include "TriggerBoard/CTB_Controller.hh"
#include "TriggerBoard/CTB_Receiver.hh"

#include <random>
#include <vector>
#include <array>
#include <atomic>
#include <chrono>

namespace dune {    

  class TriggerBoardReader : public artdaq::CommandableFragmentGenerator {
  public:
    explicit TriggerBoardReader(fhicl::ParameterSet const & ps);
    ~TriggerBoardReader();

    // useful information
    static double CTB_Clock() { return 50e6 ; }  

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

    // threads
    int _FragmentGenerator() ;

    // utilities
    artdaq::Fragment* CreateFragment() ;
    void ResetBuffer() ;
    unsigned int _rollover ; 

    //conditions to generate a Fragments
    bool CanCreateFragments() ; 
    bool NeedToEmptyBuffer() ; 

    // members
    std::unique_ptr<CTB_Controller> _run_controller ; 
    std::unique_ptr<CTB_Receiver>   _receiver ; 

    //multi thread parameters
    std::chrono::microseconds _timeout ;
    std::atomic<bool> _stop_requested ;
    std::atomic<bool> _error_state ;
    std::future<int> _frag_future ;

    boost::lockfree::spsc_queue<artdaq::Fragment* > _fragment_buffer ; 

    // fragment creation parameters
    unsigned int _group_size ;
    unsigned int _max_words_per_frag ;    

    bool _has_last_TS = false ;
    artdaq::Fragment::timestamp_t _last_timestamp = artdaq::Fragment::InvalidTimestamp ;

    bool throw_exception_;

    // metric parameters
    unsigned int _metric_TS_max ;
    unsigned int _metric_TS_counter = 0 ; 
    unsigned int _metric_Word_counter = 0 ; 

    unsigned int _metric_HLT_counter = 0 ;  // this is not the sum of the specific HLT_n, 
                                            // it is the counter of the HLT word
    unsigned int _metric_HLT_counters[8] = {0} ;
    unsigned int _metric_LLT_counter = 0 ; 

    unsigned int _metric_beam_trigger_counter = 0 ;
    unsigned int _metric_good_particle_counter = 0 ;  //LLT_1

    unsigned int _metric_CS_counter = 0 ;  // channel status

    // metric utilities
    const std::array<std::string, 8> _metric_HLT_names  = { "CTB_HLT_0_rate",
							    "CTB_HLT_1_rate", 
							    "CTB_HLT_2_rate",
							    "CTB_HLT_3_rate",
							    "CTB_HLT_4_rate",
							    "CTB_HLT_5_rate",
							    "CTB_HLT_6_rate",
							    "CTB_HLT_7_rate" } ;


  };

  inline bool TriggerBoardReader::CanCreateFragments() {

    return ( _group_size > 0  && _receiver -> N_TS_Words().load() >= _group_size ) || 
           ( _group_size <= 0 && _receiver -> Buffer().read_available() > 0 ) ;
  }

  inline bool TriggerBoardReader::NeedToEmptyBuffer() {
    
    unsigned int n_word = _receiver -> Buffer().read_available() ;
    return ( n_word >= _max_words_per_frag ) || 
           ( _receiver -> ErrorState() && n_word > 0 ) ;
  }

}

#endif /* dune_artdaq_Generators_TriggerBoardReader_hh */
