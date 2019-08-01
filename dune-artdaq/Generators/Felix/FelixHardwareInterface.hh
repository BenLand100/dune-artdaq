#ifndef dune_artdaq_Generators_Felix_FelixHardwareInterface_hh
#define dune_artdaq_Generators_Felix_FelixHardwareInterface_hh

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"
#include "dune-raw-data/Overlays/CPUHitsFragment.hh"
#include "artdaq/DAQrate/RequestReceiver.hh"
#include "fhiclcpp/fwd.h"

#include "NetioHandler.hh"
#include "RequestReceiver.hh"

#include <netio/netio.hpp>

#include <deque>
#include <random>
#include <chrono>
#include <mutex>
#include <condition_variable>

/*
 * FelixHardwareInterface
 * Authors: Roland.Sipos@cern.ch
 *          Enrico.Gamberini@cern.ch 
 * Description: Hardware interface of FELIX BoardReader for ArtDAQ
 * Date: November 2017
*/
class FelixHardwareInterface {

public:
  FelixHardwareInterface(fhicl::ParameterSet const & ps);
  ~FelixHardwareInterface();

  // Busy check of NETIO communication
  bool Busy() { return nioh_.busy(); }

  // Functionalities
  void StartDatataking();
  void StopDatataking();
  bool FillFragment( std::unique_ptr<artdaq::Fragment>& frag, std::unique_ptr<artdaq::Fragment>& fraghits );

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
    fhicl::ParameterSet tpf_params_;

    LinkParameters ( const unsigned short& id,
                     const std::string& host,
                     const unsigned short& port,
                     const unsigned short& tag,
                     fhicl::ParameterSet tpf_params):
        id_( id ),
        host_( host ),
        port_( port ),
        tag_( tag ),
        tpf_params_(tpf_params)
    { }
  };

private:
  // Configuration
  bool fake_triggers_;
  bool extract_;
  unsigned queue_size_; // be careful with IOVEC messages -> 236640 byte per message!
  unsigned message_size_; //480
  std::string backend_; // posix or fi_verbs
  bool zerocopy_;
  unsigned offset_;
  unsigned window_;
  unsigned window_offset_;
  bool reordering_;
  bool trigger_primitive_finding_;
  bool compression_;
  int qat_engine_;
  std::string requester_address_;
  std::string request_address_;
  unsigned short request_port_;
  unsigned short requests_size_;

  // NETIO & NIOH & RequestReceiver
  std::vector<LinkParameters> link_parameters_;
  NetioHandler& nioh_;
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
  dune::FragmentType fragment_hits_type_;
  dune::FelixFragmentBase::Metadata fragment_meta_;
  dune::CPUHitsFragment::Metadata fragment_hits_meta_;

  std::size_t usecs_between_sends_;
  
  using time_type = decltype(std::chrono::high_resolution_clock::now());
  const time_type fake_time_ = std::numeric_limits<time_type>::max();

  // Members needed to generate the simulated data
  time_type start_time_;
  time_type stop_time_;
  int send_calls_;
};

#endif

