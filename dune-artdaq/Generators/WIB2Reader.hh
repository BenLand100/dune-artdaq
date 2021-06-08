#ifndef __WIB2Reader_hh
#define __WIB2Reader_hh

#include "zmq.hpp"

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"

#include "dune-artdaq/Generators/wib2/wib.pb.h"
#include "Felix/RequestReceiver.hh"

namespace wib2daq {

class WIB2Reader : public artdaq::CommandableFragmentGenerator {
public:
  WIB2Reader();
  // "initialize" transition
  explicit WIB2Reader(fhicl::ParameterSet const& ps);
  // "shutdown" transition
  virtual ~WIB2Reader();

private:
  std::vector<bool> enable_FEMBs;

  // "start" transition
  void start() override;
  // "stop" transition
  void stop() override;
  
  bool acquireAsyncData(artdaq::FragmentPtrs& frags);
  void acquireSyncData(artdaq::FragmentPtrs& frags);
  bool getNext_(artdaq::FragmentPtrs& output) override;
  
  void stopNoMutex() override {}

  void setupWIB(const fhicl::ParameterSet &WIB_config);
  void setupWIBCryo(const fhicl::ParameterSet &WIB_config);
  void setupWIB3Asic(const fhicl::ParameterSet &WIB_config);
  
  template <class R, class C>
  void send_command(const C &msg, R &repl); 
  
  bool run_script(std::string name);
  
  uint8_t trigger_command = 0; //TLU command code
  uint32_t trigger_rec_ticks = 0; //4.15834 ns ticks (18 bit max)
  uint32_t trigger_timeout_ms = 0; //Maximum number of ms to wait for trigger (32 bit max)
  std::unique_ptr<RequestReceiver> request_receiver = nullptr;
  std::unique_ptr<TriggerInfo> last_trigger = nullptr;
  std::unique_ptr<wib::ReadDaqSpy::DaqSpy> last_data = nullptr;
  
  
  zmq::context_t *context = NULL;
  zmq::socket_t *socket = NULL;
  bool spy_buffer_readout = false;
  bool ignore_daq_failures = false;
  bool ignore_config_failures = false;

  bool is_cryo = false;
};

}

#endif
