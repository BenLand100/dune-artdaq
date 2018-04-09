#ifndef dune_artdaq_Generators_Felix_FelixHardwareInterface_hh
#define dune_artdaq_Generators_Felix_FelixHardwareInterface_hh

#include "dune-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/fwd.h"

#include "ProducerConsumerQueue.h"
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
  void FillFragment( std::unique_ptr<artdaq::Fragment>& frag );

  // Info
  int SerialNumber() const;
  int BoardType() const;

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
  bool fake_triggers_;
  unsigned int message_size_; //480
  std::string backend_; // posix or fi_verbs
  unsigned offset_;
  unsigned window_;
  std::string requester_address_;
  std::string multicast_address_;
  unsigned short multicast_port_;
  unsigned short requests_size_;

  // NETIO & NIOH & RequestReceiver
  std::vector<LinkParameters> link_parameters_;
  NetioHandler& nioh_;
  RequestReceiver request_receiver_;

  // Statistics and internals
  std::atomic<unsigned long long> messages_received_;
  std::atomic<unsigned long long> bytes_received_;
  std::atomic<bool> taking_data_;
  std::atomic<unsigned long long> fake_trigger_;
  std::atomic<unsigned int> fake_trigger_attempts_;

  dune::FragmentType fragment_type_;
  std::size_t usecs_between_sends_;
  
  using time_type = decltype(std::chrono::high_resolution_clock::now());
  const time_type fake_time_ = std::numeric_limits<time_type>::max();

  // Members needed to generate the simulated data
  time_type start_time_;
  time_type stop_time_;
  int send_calls_;
};

#endif

