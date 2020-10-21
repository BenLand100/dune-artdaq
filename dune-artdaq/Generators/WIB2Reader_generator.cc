#include "WIB2Reader.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "cetlib/exception.h"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

#include <wib.pb.h>

// sends metric of register value at <register name> named WIB.<register name> with <level>
// not averaged or summed, just the last value
#define sendRegMetric(name,level) artdaq::Globals::metricMan_->sendMetric(name,   (long unsigned int) wib->Read(name), "", level, artdaq::MetricMode::LastPoint, "WIB");

namespace wib2daq {

template <class R, class C>
void WIB2Reader::send_command(const C &msg, R &repl) {

    wib::Command command;
    command.mutable_cmd()->PackFrom(msg);
    
    std::string cmd_str;
    command.SerializeToString(&cmd_str);
    
    zmq::message_t request(cmd_str.size());
    memcpy((void*)request.data(), cmd_str.c_str(), cmd_str.size());
    socket.send(request,zmq::send_flags::none);
    
    zmq::message_t reply;
    socket.recv(reply,zmq::recv_flags::none);
    
    std::string reply_str(static_cast<char*>(reply.data()), reply.size());
    repl.ParseFromString(reply_str);
    
}

bool WIB2Reader::run_script(std::string name) {
    wib::Script req;
    req.set_script(name);
    req.set_file(true);
    wib::Status rep;
    send_command(*socket,req,rep);
    return rep.status();
}

// "initialize" transition
WIB2Reader::WIB2Reader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  const std::string identification = "wibdaq::WIB2Reader::WIB2Reader";

  bool success = setupWIB(ps);
  
  if (!success)
  {
    cet::exception excpt(identification);
    excpt << "Failed to configure WIB";
    throw excpt;
  }
}

void WIB2Reader::setupWIB(fhicl::ParameterSet const& ps) {

  const std::string identification = "wibdaq::WIB2Reader::setupWIB";

  auto wib_address = ps.get<std::string>("WIB.address");
  
  auto enable_FEMBs = ps.get<std::vector<bool> >("WIB.config.enable_FEMBs");
  auto FEMB_configs = ps.get<std::vector<fhicl::ParameterSet> >("WIB.config.FEMBs");
 
  if (enable_FEMBs.size() != 4)
  {
    cet::exception excpt(identification);
    excpt << "Length of WIB.config.enable_FEMBs must be 4, not: " << enable_FEMBs.size();
    throw excpt;
  }
  if (FEMB_configs.size() != 4)
  {
    cet::exception excpt(identification);
    excpt << "Length of WIB.config.FEMBs must be 4, not: " << FEMB_configs.size();
    throw excpt;
  }

  dune::DAQLogger::LogInfo(identification) << "Connecting to WIB at " <<  wib_address;
  context = new zmq::context_t(1)
  socket = new zmq::socket_t(*context, ZMQ_REQ)
  socket.connect(wib_address); // tcp://192.168.121.*:1234

  // FIXME reset and configure WIB (except FEMBs)
    wib::Initialize req;
    wib::Empty rep;
    send_command(s,req,rep);
  
  // Configure and power on FEMBs
  for(size_t iFEMB = 0; iFEMB < 4; iFEMB++)
  {
    if(enable_FEMBs[iFEMB])
    {
      fhicl::ParameterSet const& FEMB_config = FEMB_configs[iFEMB];
      setupFEMB(iFEMB, FEMB_config);
    }
    else
    {
      dune::DAQLogger::LogInfo(identification) << "FEMB"<<(iFEMB+1)<<" not enabled";
    }
  }
  
  dune::DAQLogger::LogInfo(identification) << "Configured WIB";
  
}


void WIB2Reader::setupFEMB(size_t iFEMB, fhicl::ParameterSet const& FEMB_config) {

  const std::string identification = "wibdaq::WIB2Reader::setupFEMB";
  
  // FIXME power on FEMB
  run_script("femb"+to_string(iFEMB)+"_on");

  // FIXME configure FEMB
  run_script("femb"+to_string(iFEMB)+"_conf");
  
}

// "shutdown" transition
WIB2Reader::~WIB2Reader() {

}

// "start" transition
void WIB2Reader::start() {
  const std::string identification = "wibdaq::WIB2Reader::start";
  if (!wib) 
  {
    cet::exception excpt(identification);
    excpt << "WIB object pointer NULL";
    throw excpt;
  }
}

// "stop" transition
void WIB2Reader::stop() {
  const std::string identification = "wibdaq::WIB2Reader::stop";
}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIB2Reader::getNext_(artdaq::FragmentPtrs& /*frags*/) {
  // FIXME why did protodune sleep here???
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  return (! should_stop()); // returning false before should_stop makes all other BRs stop
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wib2daq::WIB2Reader)
