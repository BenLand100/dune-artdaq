#ifndef dune_artdaq_Generators_Felix_FelixOnHostInterface_hh
#define dune_artdaq_Generators_Felix_FelixOnHostInterface_hh

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "artdaq/DAQrate/RequestReceiver.hh"
#include "fhiclcpp/fwd.h"

#include "NetioHandler.hh"
#include "QueueHandler.hh"
#include "RequestReceiver.hh"

#include "dune-artdaq/Generators/Felix/FelixProtoduneReader.hpp"

#include <netio/netio.hpp>

#include <deque>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>

/*
 * FelixOnHostInterface
 * Authors: Roland.Sipos@cern.ch
 * Description: HW interface for OnHost FELIX BoardReader for ArtDAQ
 * Date: November 2017
*/
class FelixOnHostInterface {

public:
  FelixOnHostInterface(fhicl::ParameterSet const & ps);
  ~FelixOnHostInterface();

  // Busy check of NETIO communication
  bool Busy();
  bool SetupTriggerMatchers();
  void LockTriggerMatchers();
  bool TriggerWorkers(artdaq::FragmentPtrs& frags);

  void RecalculateFragmentSizes();

  // Functionalities
  void StartDatataking();
  void StopDatataking();
  bool FillFragments( artdaq::FragmentPtrs& frags );

  // Info
  int SerialNumber() const;
  int BoardType() const;
  unsigned MessageSize() const;
  unsigned TriggerWindowSize() const;
  unsigned TriggerWindowOffset() const;

  // Inner structures
  struct LinkParameters
  {
    unsigned short id_;
    std::string host_;
    unsigned short port_;
    unsigned short tag_;    

    LinkParameters ( const unsigned short& id,
                     const std::string& host,
                     const unsigned short& port,
                     const unsigned short& tag ):
        id_( id ),
        host_( host ),
        port_( port ),
        tag_( tag )
    { }
  };

private:
  // Configuration
  CPUPin& cpu_pin_;
  std::string pin_file_;

  bool fake_triggers_;
  bool extract_;
  unsigned queue_size_; // be careful with IOVEC messages -> 236640 byte per message!
  unsigned message_size_; //480
  unsigned frame_size_;
  std::string backend_; // posix or fi_verbs
  bool zerocopy_;
  unsigned offset_;
  unsigned window_;
  unsigned window_offset_;
  bool reordering_;
  bool compression_;
  int qat_engine_;
  std::string requester_address_;
  std::string request_address_;
  unsigned short request_port_;
  unsigned short requests_size_;

  // OnHost mode
  QueueHandler& queh_;
  std::map<unsigned, std::unique_ptr<ProtoDuneReader>> card_readers_;
  std::map<unsigned, std::map<uint32_t, uint32_t>> link_info_;
  std::map<unsigned, std::thread> parser_threads_;
  std::vector<std::thread> consumer_threads_;

  // TriggerMatchers
  std::vector<std::unique_ptr<ReusableThread>> trm_extractors_;
  std::vector<std::function<void()>> trm_functors_;
  std::vector<artdaq::Fragment*> frag_ptrs_;
  // Thread control
  std::atomic<bool> stop_trigger_;
  // Trigger
  uint64_t trigger_ts_;
  uint64_t trigger_seq_id_;
  std::vector<uint_fast64_t> last_tss_;
  const uint_fast64_t tick_dist_=25;

  size_t flx_queue_size_;
  size_t num_sources_;
  size_t num_links_;
  size_t card_offset_;
  uint32_t felix_id_;

  // NETIO & NIOH
  std::vector<LinkParameters> link_parameters_;
//  NetioHandler& nioh_;

  // RequestReceiver
  std::unique_ptr<RequestReceiver> request_receiver_;

  // Statistics and internals
  std::atomic<unsigned long long> messages_received_;
  std::atomic<unsigned long long> bytes_received_;
  std::atomic<bool> taking_data_;
  std::atomic<bool> first_datataking_;
  std::atomic<unsigned long long> fake_trigger_;
  std::atomic<unsigned int> fake_trigger_attempts_;

  // Fragment related
  dune::FragmentType fragment_type_;
  dune::FelixFragmentBase::Metadata fragment_meta_;
  size_t m_timeWindowByteSizeIn;
  size_t m_timeWindowByteSizeOut;
  size_t m_timeWindowNumFrames;
  size_t m_timeWindowNumMessages; 

  // Reordering & Compression
  std::unique_ptr<ReorderFacility> m_reorderFacility;
  std::unique_ptr<QzCompressor> m_compressionFacility;

  std::size_t usecs_between_sends_;
  
  using time_type = decltype(std::chrono::high_resolution_clock::now());
  const time_type fake_time_ = std::numeric_limits<time_type>::max();

  // Members needed to generate the simulated data
  time_type start_time_;
  time_type stop_time_;
  int send_calls_;
};

#endif

