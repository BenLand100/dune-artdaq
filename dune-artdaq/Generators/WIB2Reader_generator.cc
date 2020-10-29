#include "WIB2Reader.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "cetlib/exception.h"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

#include "dune-artdaq/Generators/wib2/wib.pb.h"

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
    socket->send(request);
    
    zmq::message_t reply;
    socket->recv(&reply);
    
    std::string reply_str(static_cast<char*>(reply.data()), reply.size());
    repl.ParseFromString(reply_str);
    
}

bool WIB2Reader::run_script(std::string name) {
    wib::Script req;
    req.set_script(name);
    req.set_file(true);
    wib::Status rep;
    send_command(req,rep);
    return rep.success();
}

// "initialize" transition
WIB2Reader::WIB2Reader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  const std::string identification = "wibdaq::WIB2Reader::WIB2Reader";

  setupWIB(ps);
}

void WIB2Reader::setupWIB(const fhicl::ParameterSet &ps) {

  const std::string identification = "wibdaq::WIB2Reader::setupWIB";
  
  spy_buffer_readout = ps.get<bool>("WIB.config.spy_buffer_readout");
  ignore_daq_failures = ps.get<bool>("WIB.config.ignore_daq_failures");
 
  auto wib_address = ps.get<std::string>("WIB.config.address");
 
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
  context = new zmq::context_t(1);
  dune::DAQLogger::LogInfo(identification) << "ZMQ context initialized";
  socket = new zmq::socket_t(*context, ZMQ_REQ);
  dune::DAQLogger::LogInfo(identification) << "ZMQ socket initialized";
  socket->connect(wib_address); // tcp://192.168.121.*:1234
  dune::DAQLogger::LogInfo(identification) << "Connected!";

  wib::ConfigureWIB req;
  // FIXME populate fields to send to WIB
  wib::Status rep;
  dune::DAQLogger::LogInfo(identification) << "Sending ConfigureWIB command";
  send_command(req,rep);
  
  if (!rep.success())
  {
    cet::exception excpt(identification);
    excpt << "Failed to configure WIB";
    throw excpt;
  }

  dune::DAQLogger::LogInfo(identification) << "Configuring FEMBs";
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
      wib::ConfigureFEMB req;
      req.set_index(iFEMB);
      req.set_enabled(false);
      wib::Status rep;
      send_command(req,rep);  
      if (!rep.success())
      {
        cet::exception excpt(identification);
        excpt << "Failed to configure FEMB"<<(iFEMB+1);
        throw excpt;
      }
      dune::DAQLogger::LogInfo(identification) << "FEMB"<<(iFEMB+1)<<" not enabled";
    }
  }
  
  dune::DAQLogger::LogInfo(identification) << "Configured WIB";
  
}


void WIB2Reader::setupFEMB(size_t iFEMB, const fhicl::ParameterSet &FEMB_config) {

  const std::string identification = "wibdaq::WIB2Reader::setupFEMB";  

  wib::ConfigureFEMB req;
  req.set_index(iFEMB);
  req.set_enabled(true);
  //FIXME populate fields to send to WIB
  wib::Status rep;
  send_command(req,rep);  
  if (!rep.success())
  {
    cet::exception excpt(identification);
    excpt << "Failed to configure FEMB"<<(iFEMB+1);
    throw excpt;
  }
  
  dune::DAQLogger::LogInfo(identification) << "Configured FEMB"<<(iFEMB+1);
  
}

// "shutdown" transition
WIB2Reader::~WIB2Reader() {
    socket->close();
    delete socket;
    delete context;
}

// "start" transition
void WIB2Reader::start() {
  const std::string identification = "wibdaq::WIB2Reader::start";
}

// "stop" transition
void WIB2Reader::stop() {
  const std::string identification = "wibdaq::WIB2Reader::stop";
}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIB2Reader::getNext_(artdaq::FragmentPtrs& frags) {
  if (!spy_buffer_readout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return (! should_stop());
  } else {
    if (should_stop()) return false;
    wib::ReadDaqSpy req;
    req.set_buf0(true);
    req.set_buf1(true);
    wib::DaqSpy rep;	
    send_command(req,rep);  
   
	std::string metadata("meta");
    size_t bytes_read = rep.buf0().size() + rep.buf1().size();
    std::unique_ptr<artdaq::Fragment> fragptr(
   					    artdaq::Fragment::FragmentBytes(bytes_read,  
   									    ev_counter(), fragment_id(),
   									    /*FIXME*/42,metadata));

    memcpy(fragptr->dataBeginBytes(), rep.buf0().c_str(), rep.buf0().size());
    memcpy(fragptr->dataBeginBytes()+rep.buf0().size() , rep.buf1().c_str(), rep.buf1().size());

    //FIXME this crashes
	//frags.emplace_back(std::move(fragptr));
    
    return ignore_daq_failures || rep.success();
  }
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wib2daq::WIB2Reader)
