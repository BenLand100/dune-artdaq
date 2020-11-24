#include "WIB2Reader.hh"
#include "artdaq/Generators/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/Frame14Fragment.hh"
#include "cetlib/exception.h"

#include <sstream>
#include <vector>
#include <memory>
#include <iomanip>
#include <chrono>
#include <thread>

#include "dune-artdaq/Generators/wib2/wib.pb.h"

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
  ignore_config_failures = ps.get<bool>("WIB.config.ignore_config_failures");
  ignore_daq_failures = ps.get<bool>("WIB.config.ignore_daq_failures");
  enable_pulser = ps.get<bool>("WIB.config.enable_pulser");
  frontend_cold = ps.get<bool>("WIB.config.frontend_cold");
 
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
  req.set_cold(frontend_cold);

  dune::DAQLogger::LogInfo(identification) << "Configuring FEMBs";
  for(size_t iFEMB = 0; iFEMB < 4; iFEMB++)
  {
    wib::ConfigureWIB::ConfigureFEMB *femb_conf = req.add_fembs();
    fhicl::ParameterSet const& femb_fhicl = FEMB_configs[iFEMB];
    
    femb_conf->set_enabled(enable_FEMBs[iFEMB]);
    
    femb_conf->set_test_cap(femb_fhicl.get<bool>("test_cap"));
    femb_conf->set_gain(femb_fhicl.get<uint32_t>("gain"));
    femb_conf->set_peak_time(femb_fhicl.get<uint32_t>("peak_time"));
    femb_conf->set_baseline(femb_fhicl.get<uint32_t>("baseline"));
    femb_conf->set_pulse_dac(femb_fhicl.get<uint32_t>("pulse_dac"));
    
    femb_conf->set_leak(femb_fhicl.get<uint32_t>("leak"));
    femb_conf->set_leak_10x(femb_fhicl.get<bool>("leak_10x"));
    femb_conf->set_ac_couple(femb_fhicl.get<bool>("ac_couple"));
    femb_conf->set_buffer(femb_fhicl.get<uint32_t>("buffer"));
    
    femb_conf->set_strobe_skip(femb_fhicl.get<uint32_t>("strobe_skip"));
    femb_conf->set_strobe_delay(femb_fhicl.get<uint32_t>("strobe_delay"));
    femb_conf->set_strobe_length(femb_fhicl.get<uint32_t>("strobe_length"));
  }
 
  dune::DAQLogger::LogInfo(identification) << "Sending ConfigureWIB command";
  wib::Status rep;
  send_command(req,rep);
  
  if (!rep.success() && !ignore_config_failures)
  {
    cet::exception excpt(identification);
    excpt << "Failed to configure WIB";
    throw excpt;
  }
  
  dune::DAQLogger::LogInfo(identification) << "Configured WIB";
  
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
  if (enable_pulser) {
      wib::Pulser req;
      req.set_start(true);
      wib::Empty rep;
      send_command(req,rep);
  }
}

// "stop" transition
void WIB2Reader::stop() {
  const std::string identification = "wibdaq::WIB2Reader::stop";
  if (enable_pulser) {
      wib::Pulser req;
      req.set_start(false);
      wib::Empty rep;
      send_command(req,rep);
  }
}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIB2Reader::getNext_(artdaq::FragmentPtrs& frags) {
  const std::string identification = "wibdaq::WIB2Reader::getNext_";
  if (!spy_buffer_readout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
	return (! should_stop());
  } else {
    if (should_stop()) return false;
    wib::ReadDaqSpy req;
    req.set_buf0(true);
    req.set_buf1(true);
    wib::ReadDaqSpy::DaqSpy rep;	
    send_command(req,rep);  
    {   //buf0
        dune::Frame14Fragment::Metadata meta;
        meta.control_word  = 0xdef; //FIXME
        meta.version = 0; //FIXME
        meta.reordered = 0;
        meta.compressed = 0;
        meta.num_frames = rep.buf0().size() / sizeof(dune::frame14::frame14);
        meta.offset_frames = 0; //FIXME what is this
        meta.window_frames = 0; //FIXME what is this
        std::unique_ptr<artdaq::Fragment> fragptr(
       					    artdaq::Fragment::FragmentBytes(rep.buf0().size(),  
       									    ev_counter(), fragmentIDs()[0],
       									    dune::detail::FragmentType::FRAME14,meta));
        dune::DAQLogger::LogInfo(identification) << "Created fragment " << ev_counter() << " " << fragmentIDs()[0] << " " << rep.buf0().size();
        memcpy(fragptr->dataBeginBytes(), rep.buf0().c_str(), rep.buf0().size());
        frags.emplace_back(std::move(fragptr));
    }
    {   //buf1
        dune::Frame14Fragment::Metadata meta;
        meta.control_word  = 0xdef; //FIXME
        meta.version = 0; //FIXME
        meta.reordered = 0;
        meta.compressed = 0;
        meta.num_frames = rep.buf0().size() / sizeof(dune::frame14::frame14);
        meta.offset_frames = 0; //FIXME what is this
        meta.window_frames = 0; //FIXME what is this
        std::unique_ptr<artdaq::Fragment> fragptr(
       					    artdaq::Fragment::FragmentBytes(rep.buf0().size(),  
       									    ev_counter(), fragmentIDs()[1],
       									    dune::detail::FRAME14,meta));
        dune::DAQLogger::LogInfo(identification) << "Created fragment " << ev_counter() << " " <<fragmentIDs()[1] << " " << rep.buf0().size();
        memcpy(fragptr->dataBeginBytes(), rep.buf0().c_str(), rep.buf0().size());
        frags.emplace_back(std::move(fragptr));
    }
    
    ev_counter_inc(); 
    return ignore_daq_failures || rep.success();
  }
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wib2daq::WIB2Reader)
