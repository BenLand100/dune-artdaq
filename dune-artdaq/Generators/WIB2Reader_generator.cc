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
  trigger_command = ps.get<uint8_t>("WIB.config.trigger_command");
  trigger_rec_ticks = ps.get<uint32_t>("WIB.config.trigger_rec_ticks");
  trigger_timeout_ms = ps.get<uint32_t>("WIB.config.trigger_timeout_ms");
  auto timing_address = ps.get<std::string>("WIB.config.timing_address");
  if (timing_address.length() > 0) {
    request_receiver = std::make_unique<RequestReceiver>(timing_address);
  }
  ignore_config_failures = ps.get<bool>("WIB.config.ignore_config_failures");
  ignore_daq_failures = ps.get<bool>("WIB.config.ignore_daq_failures");
  enable_pulser = ps.get<bool>("WIB.config.enable_pulser");
  frontend_cold = ps.get<bool>("WIB.config.frontend_cold");
 
  auto wib_address = ps.get<std::string>("WIB.config.address");
 
  enable_FEMBs = ps.get<std::vector<bool> >("WIB.config.enable_FEMBs");
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
  req.set_pulser(enable_pulser);

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
  if (request_receiver) request_receiver->start();
}

// "stop" transition
void WIB2Reader::stop() {
  const std::string identification = "wibdaq::WIB2Reader::stop";
  if (request_receiver) request_receiver->stop();
}

int cmp_buf_to_ts(const char *buf, size_t size, uint64_t ts) {
  dune::frame14::frame14 const* frame = (dune::frame14::frame14 const*)buf;
  const size_t n = size / sizeof(dune::frame14::frame14);
  const uint64_t start = frame[0].timestamp;
  const uint64_t end = frame[n-1].timestamp;
  dune::DAQLogger::LogInfo("cmp_buf_to_ts") << "ts: " << ts << " start: " << start << " end: " << end;
  if (ts <= end && ts >= start) {
    return 0; // buf contains ts
  } else if (ts < start) {
    return -1; // ts is before buf
  } else {
    return 1; // ts is after buf
  }
}

std::unique_ptr<artdaq::Fragment> buf_to_frag(const char *buf, size_t size, int frag_id, int ev_id) {
  dune::frame14::frame14 const* frame = (dune::frame14::frame14 const*)buf;
  dune::Frame14Fragment::Metadata meta;
  meta.control_word  = 0xdef; //FIXME
  meta.version = frame->frame_version; // all versions have this in the same place
  meta.reordered = 0;
  meta.compressed = 0;
  meta.num_frames = size / sizeof(dune::frame14::frame14);
  meta.offset_frames = 0; //FIXME what is this
  meta.window_frames = 0; //FIXME what is this
  std::unique_ptr<artdaq::Fragment> fragptr(
 					    artdaq::Fragment::FragmentBytes(size, ev_id, frag_id,
 									    dune::detail::FragmentType::FRAME14,meta));
  memcpy(fragptr->dataBeginBytes(), buf, size);
  return std::move(fragptr);
}

std::unique_ptr<artdaq::Fragment> empty_frag(int frag_id, int ev_id) {
  dune::Frame14Fragment::Metadata meta;
  meta.control_word  = 0xdef; //FIXME
  meta.version = 0;
  meta.reordered = 0;
  meta.compressed = 0;
  meta.num_frames = 0;
  meta.offset_frames = 0; //FIXME what is this
  meta.window_frames = 0; //FIXME what is this
  std::unique_ptr<artdaq::Fragment> fragptr(
 					    artdaq::Fragment::FragmentBytes(0, ev_id, frag_id,
 									    dune::detail::FragmentType::FRAME14,meta));
  return std::move(fragptr);
}

int cmp_spydaq_to_ts(const std::unique_ptr<wib::ReadDaqSpy::DaqSpy> &last_data, uint64_t ts) {
  return last_data->buf0().size() ? 
              cmp_buf_to_ts(last_data->buf0().c_str(),last_data->buf0().size(),ts) : 
              cmp_buf_to_ts(last_data->buf1().c_str(),last_data->buf1().size(),ts) ;
}

void WIB2Reader::acquireSyncData(artdaq::FragmentPtrs& frags) {
  const std::string identification = "wibdaq::WIB2Reader::acquireSyncData";
  if (!last_trigger) {
    TriggerInfo request = request_receiver->getNextRequest();
    last_trigger = std::make_unique<TriggerInfo>(request);
    dune::DAQLogger::LogInfo(identification) << "Received request " << last_trigger->seqID << " at timestamp " << last_trigger->timestamp;
  }
  if (!last_data) {
    wib::ReadDaqSpy req;
    req.set_buf0(enable_FEMBs[0] || enable_FEMBs[1]);
    req.set_buf1(enable_FEMBs[2] || enable_FEMBs[3]);
    req.set_trigger_command(trigger_command);
    req.set_trigger_rec_ticks(trigger_rec_ticks);
    req.set_trigger_timeout_ms(trigger_timeout_ms);
    wib::ReadDaqSpy::DaqSpy rep;	
    send_command(req,rep);  
    if (rep.success()) {
      last_data = std::make_unique<wib::ReadDaqSpy::DaqSpy>(rep);
      dune::DAQLogger::LogInfo(identification) << "Acquired data from WIB";
    } else {
      dune::DAQLogger::LogInfo(identification) << "Acquisition from WIB failed";
      return; // try again next time
    }
  }
  
  const uint64_t req_ts = last_trigger->timestamp;
  const uint64_t req_id = last_trigger->seqID;
  const int cmp = cmp_spydaq_to_ts(last_data,req_ts);
  if (cmp == 0) { // trigger is in this data window
    if (enable_FEMBs[0] || enable_FEMBs[1]) {   //buf0
      std::unique_ptr<artdaq::Fragment> fragptr = buf_to_frag(last_data->buf0().c_str(),last_data->buf0().size(),fragmentIDs()[0],req_id);
      dune::DAQLogger::LogInfo(identification) << "Created fragment " << req_id << " " <<fragmentIDs()[0] << " " << last_data->buf0().size();
      frags.emplace_back(std::move(fragptr));
    } else {
      std::unique_ptr<artdaq::Fragment> fragptr = empty_frag(fragmentIDs()[0],req_id);
      frags.emplace_back(std::move(fragptr));
    }
    if (enable_FEMBs[2] || enable_FEMBs[3]) {   //buf1
      std::unique_ptr<artdaq::Fragment> fragptr = buf_to_frag(last_data->buf1().c_str(),last_data->buf1().size(),fragmentIDs()[1],req_id);
      dune::DAQLogger::LogInfo(identification) << "Created fragment " << req_id << " " <<fragmentIDs()[1] << " " << last_data->buf1().size();
      frags.emplace_back(std::move(fragptr));
    } else {
      std::unique_ptr<artdaq::Fragment> fragptr = empty_frag(fragmentIDs()[1],req_id);
      frags.emplace_back(std::move(fragptr));
    }
    last_data = nullptr;
    last_trigger = nullptr;
  } else if (cmp > 0) { // trigger is in the future
    dune::DAQLogger::LogInfo(identification) << "Discarding stale data";
    last_data = nullptr; // throw away the stale data
  } else { // trigger is in the past
    dune::DAQLogger::LogInfo(identification) << "Discarding stale trigger";
    last_trigger = nullptr; // throw away the stale trigger
  } 
}


bool WIB2Reader::acquireAsyncData(artdaq::FragmentPtrs& frags) {
  const std::string identification = "wibdaq::WIB2Reader::acquireAsyncData";
  wib::ReadDaqSpy req;
  req.set_buf0(enable_FEMBs[0] || enable_FEMBs[1]);
  req.set_buf1(enable_FEMBs[2] || enable_FEMBs[3]);
  //trigger_command should probably be 0, but allow triggers without a timing component ("Sync")
  req.set_trigger_command(trigger_command);
  req.set_trigger_rec_ticks(trigger_rec_ticks);
  req.set_trigger_timeout_ms(trigger_timeout_ms);
  wib::ReadDaqSpy::DaqSpy rep;	
  send_command(req,rep);  
  if (rep.success() && (enable_FEMBs[0] || enable_FEMBs[1])) {   //buf0
    std::unique_ptr<artdaq::Fragment> fragptr = buf_to_frag(rep.buf0().c_str(),rep.buf0().size(),fragmentIDs()[0],ev_counter());
    dune::DAQLogger::LogInfo(identification) << "Created fragment " << ev_counter() << " " <<fragmentIDs()[0] << " " << rep.buf0().size();
    frags.emplace_back(std::move(fragptr));
  } else {
    std::unique_ptr<artdaq::Fragment> fragptr = empty_frag(fragmentIDs()[0],ev_counter());
    frags.emplace_back(std::move(fragptr));
  }
  if (rep.success() && (enable_FEMBs[2] || enable_FEMBs[3])) {   //buf1
    std::unique_ptr<artdaq::Fragment> fragptr = buf_to_frag(rep.buf1().c_str(),rep.buf1().size(),fragmentIDs()[1],ev_counter());
    dune::DAQLogger::LogInfo(identification) << "Created fragment " << ev_counter() << " " <<fragmentIDs()[1] << " " << rep.buf1().size();
    frags.emplace_back(std::move(fragptr));
  } else {
    std::unique_ptr<artdaq::Fragment> fragptr = empty_frag(fragmentIDs()[1],ev_counter());
    frags.emplace_back(std::move(fragptr));
  }
  ev_counter_inc(); 
  return rep.success();
}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIB2Reader::getNext_(artdaq::FragmentPtrs& frags) {
  const std::string identification = "wibdaq::WIB2Reader::getNext_";
  if (!spy_buffer_readout) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return (! should_stop());
  } else if (request_receiver) {
    if (should_stop()) return false;
    acquireSyncData(frags);
    return true;
  } else {
    if (should_stop()) return false;
    bool success = acquireAsyncData(frags);
    return ignore_daq_failures || success;
  }
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wib2daq::WIB2Reader)
