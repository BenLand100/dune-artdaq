#ifndef __WIB2Reader_hh
#define __WIB2Reader_hh

#include "zmq.hpp"

#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Generators/CommandableFragmentGenerator.hh"


namespace wib2daq {

class WIB2Reader : public artdaq::CommandableFragmentGenerator {
public:
  WIB2Reader();
  // "initialize" transition
  explicit WIB2Reader(fhicl::ParameterSet const& ps);
  // "shutdown" transition
  virtual ~WIB2Reader();

private:
  // "start" transition
  void start() override;
  // "stop" transition
  void stop() override;
  bool getNext_(artdaq::FragmentPtrs& output) override;
  void stopNoMutex() override {}

  void setupWIB(const fhicl::ParameterSet &WIB_config);
  
  template <class R, class C>
  void send_command(const C &msg, R &repl); 
  
  bool run_script(std::string name);

  zmq::context_t *context = NULL;
  zmq::socket_t *socket = NULL;
  bool spy_buffer_readout = false;
  bool ignore_daq_failures = false;
};

}

#endif
