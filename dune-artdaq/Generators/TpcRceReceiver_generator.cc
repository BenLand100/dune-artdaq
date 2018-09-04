#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "artdaq/DAQdata/Globals.hh"
#include "dune-artdaq/Generators/TpcRceReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

namespace dune {

TpcRceReceiver::Stats::Stats() : 
   last_update(boost::posix_time::microsec_clock::local_time())
{
}

void TpcRceReceiver::Stats::track_size(size_t bytes)
{
   if (_1st_frag) {
      _min_size = bytes;
      _max_size = bytes;
      _1st_frag = false;
   }
   else {
      if (bytes > _max_size)
         _max_size = bytes;

      if (bytes < _min_size)
         _min_size = bytes;
   }
}

void TpcRceReceiver::Stats::reset_track_size()
{
   max_size  = _max_size * 1e-6; // in MB
   min_size  = _min_size * 1e-6; // in MB
   _1st_frag = true;
   _min_size = 0;
   _max_size = 0;
}

TpcRceReceiver::Timer::Timer():
   _start(boost::posix_time::microsec_clock::local_time())
{
}

bool TpcRceReceiver::Timer::lap(
      const boost::posix_time::ptime now, 
      float threshold)
{
   //auto now = boost::posix_time::microsec_clock::local_time();
   auto diff = now - _start;
   _lapse = diff.total_milliseconds() * 1e-3;

   if (_lapse > threshold) {
      _start = now;
      return true;
   }

   return false;
}

TpcRceReceiver::TpcRceReceiver(fhicl::ParameterSet const& ps) :
   CommandableFragmentGenerator(ps)
{

   typedef std::string str;
   _rce_host_addr = ps.get<str>  ("rce_client_host_addr"                );
   _rce_host_port = ps.get<int>  ("rce_client_host_port"  , 8090        );
   _rce_run_mode  = ps.get<str>  ("rce_daq_mode"          , "External"  );
   _rce_xml_file  = ps.get<str>  ("rce_xml_config_file"   , ""          );
   _rce_feb_emul  = ps.get<bool> ("rce_feb_emulation_mode", false       );
   _partition     = ps.get<int>  ("partition_number"                    );
   _daq_host_port = ps.get<str>  ("receive_port"          , "7991"      );
   _enable_rssi   = ps.get<bool> ("enable_rssi"           , true        );
   _frag_id       = ps.get<int>  ("fragment_id"                         );
   _duration      = ps.get<int>  ("duration"              , 5000        );
   _pretrigger    = ps.get<int>  ("pretrigger"            , 2500        );
   _hls_mask      = ps.get<int>  ("hls_mask"              , 0           );

   _daq_host_addr = boost::asio::ip::host_name();

   int board_id = ps.get<int>("board_id", 0);

   std::stringstream ss;
   ss << "COB" << board_id / 8 + 1
      << "-RCE" << board_id % 8 + 1;
   _instance_name = ss.str();

   // set name for metrics reporting
   metricsReportingInstanceName(_instance_name);

   // TODO check partition

   // config RCE
   try {
      DAQLogger::LogInfo(_instance_name) <<
         "Connecting to " << _rce_host_addr << ":" << _rce_host_port;
      _rce_comm.reset(new rce::RceComm(_rce_host_addr, _rce_host_port));

      DAQLogger::LogInfo(_instance_name) << "Configuring ...";
      _rce_comm->reset         ( _rce_xml_file  );
      _rce_comm->blowoff_hls   ( 0x3            );
      _rce_comm->blowoff_wib   ( true           );
      _rce_comm->set_run_mode  ( _rce_run_mode  );
      _rce_comm->set_emul      ( _rce_feb_emul  );
      _rce_comm->set_partition ( _partition     );
      _rce_comm->enable_rssi   ( _enable_rssi   );
      _rce_comm->set_daq_host  ( _daq_host_addr );
      _rce_comm->set_readout   ( _duration,
                                 _pretrigger    );
   }
   catch (const std::exception &e) {
      DAQLogger::LogError(_instance_name) << e.what();
   }

   // timeout for 0,5s
   std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void TpcRceReceiver::start(void) 
{
   DAQLogger::LogInfo(_instance_name) << "start()";

   // validate config

   // connect receiver
   _receiver.reset(new rce::RssiReceiver (_rce_host_addr, 8192));
  
   // drain buffer

   // re-enable wib and hls
   try {
      _rce_comm->blowoff_wib( false       );
      _rce_comm->blowoff_hls( _hls_mask   );
   }
   catch (const std::exception &e)
   {
      DAQLogger::LogWarning(_instance_name) << e.what();
   }

   // check status
   _check_status();

   // enable data transfer
   try {
      _rce_comm->send_cmd("SetRunState", "Enable");
   }
   catch (const std::exception &e)
   {
      DAQLogger::LogWarning(_instance_name) << e.what();
   }
}

void TpcRceReceiver::stop(void)
{
   _print_summary("End of run summary");
   _send_summary();

   DAQLogger::LogInfo(_instance_name) << "stop()";
   try {
      _rce_comm->blowoff_hls( 0x3  );
      _rce_comm->blowoff_wib( true );
      _rce_comm->send_cmd("SetRunState", "Stopped");
   }
   catch (const std::exception &e)
   {
      DAQLogger::LogWarning(_instance_name) << e.what();
   }
}

void TpcRceReceiver::stopNoMutex(void)
{
   DAQLogger::LogInfo(_instance_name) << "stopNoMutex()";
   stop();
}

bool TpcRceReceiver::getNext_(artdaq::FragmentPtrs& frags)
{
   dune::rce::BufferPtr buf;
   if (_receiver->pop(buf))
   {
      // an unique pointer, take ownership of buf
      artdaq::FragmentPtr frag(buf);


      auto *data_ptr = buf->dataBeginBytes();
      // FIXME
      uint64_t header = *reinterpret_cast<uint64_t *>(data_ptr + 12);
      
      // find header
      /*
      uint64_t header = *reinterpret_cast<uint64_t *>(data_ptr);
      size_t n_words = sizeof(uint64_t) / sizeof(artdaq::Fragment::byte_t);
      for (size_t i = 0; i < n_words; ++i) {
         if (*data_ptr != 0) {
            header = *reinterpret_cast<uint64_t *>(data_ptr);
            break;
         }
         ++data_ptr;
      }
      */

      // Set fragment fields appropriately
      frag->setSequenceID ( ev_counter()      );
      frag->setFragmentID ( _frag_id          );
      frag->setUserType   ( dune::detail::TPC );

      // debug info
      //TLOG(50, "RCE")
      _debug_ss.str("");
      _debug_ss
         << "[" << _instance_name << "] "
         << "frag id:"   << frag->fragmentID()    << " "
         << "seq id:"    << frag->sequenceID()    << " "
         << "size:"      << frag->dataSizeBytes() << " "
         << std::hex
         << "header:"    << header                << " "
         << "timestamp:" << frag->timestamp()
         << std::dec;

      DAQLogger::LogInfo(_instance_name) << _debug_ss.str();
      _last_frag = _debug_ss.str();

      // increment the event counter
      ev_counter_inc();
      ++_frag_cnt;

      // track fragment size
      _stats.track_size(frag->dataSizeBytes());

      // add the fragment to the list
      frags.emplace_back(std::move(frag));
   }
   else {
      boost::this_thread::sleep(boost::posix_time::milliseconds(50));
   }


   auto now = boost::posix_time::microsec_clock::local_time();
   // update / send stats every second
   if (_timer_stats.lap(now, 1)) {
      _update_stats();
      _send_stats();
   }

   if (_timer_summary.lap(now, 10)) {
      _send_summary();
   }

  bool is_stop = should_stop() && _receiver->read_available() == 0;
  return !is_stop;
}

// JCF, Dec-12-2015

// startOfDatataking() will figure out whether we've started taking
// data either by seeing that the run number's incremented since the
// last time it was called (implying the start transition's occurred)
// or the subrun number's incremented (implying the resume
// transition's occurred). On the first call to the function, the
// "last checked" values of run and subrun are set to 0, guaranteeing
// that the first call will return true

bool TpcRceReceiver::startOfDatataking()
{

  static int subrun_at_last_check = 0;
  static int run_at_last_check = 0;

  bool is_start = false;

  if ((run_number() > run_at_last_check) ||
      (subrun_number() > subrun_at_last_check)) {
    is_start = true;
  }

  subrun_at_last_check = subrun_number();
  run_at_last_check = run_number();

  return is_start;
}

void TpcRceReceiver::_update_stats()
{
   auto curr = _receiver->get_stats();

   auto now    = boost::posix_time::microsec_clock::local_time();
   auto dt     = now - _stats.last_update;
   float lapse = dt.total_milliseconds() * 1e-3;

   auto rx_cnt   = curr.rx_cnt   - _stats.prev.rx_cnt;
   auto rx_bytes = curr.rx_bytes - _stats.prev.rx_bytes;

   _stats.evt_rate  = lapse > 0    ? rx_cnt / lapse : 0;
   _stats.data_rate = lapse > 0    ? rx_bytes / lapse * 8e-9 : 0;
   _stats.avg_size  = rx_cnt > 0 ? rx_bytes / rx_cnt * 1e-6 : 0;
   _stats.rssi_drop = curr.rssi_drop - _stats.prev.rssi_drop;
   _stats.pack_drop = curr.pack_drop - _stats.prev.pack_drop;
   _stats.bad_hdrs  = curr.bad_hdrs  - _stats.prev.bad_hdrs;
   _stats.err_cnt   = curr.err_cnt   - _stats.prev.err_cnt;
   _stats.overflow  = curr.overflow  - _stats.prev.overflow;
   _stats.is_open   = static_cast<int>(_receiver->is_open());

   _stats.reset_track_size();

   _stats.last_update = now;
   _stats.prev        = curr;
}

void TpcRceReceiver::_print_stats() const
{
   auto last_update = to_simple_string(_stats.last_update);

   DAQLogger::LogInfo(_instance_name)
      << "=== Stats ===\n"
      << "Last Update     : " << last_update       << "\n"
      << "Rate (Hz)       : " << _stats.evt_rate   << "\n"
      << "Data (Gbps)     : " << _stats.data_rate  << "\n"
      << "Avg. Size (MB)  : " << _stats.avg_size   << "\n"
      << "Min. Size (MB)  : " << _stats.min_size   << "\n"
      << "Max. Size (MB)  : " << _stats.max_size   << "\n"
      << "RSSI Drop       : " << _stats.rssi_drop  << "\n"
      << "Pkt. Drop       : " << _stats.pack_drop  << "\n"
      << "Bad Headers     : " << _stats.bad_hdrs   << "\n"
      << "Err Cnt         : " << _stats.err_cnt    << "\n"
      << "Overflow        : " << _stats.overflow   << "\n"
      << "IsOpen          : " << _stats.is_open    << "\n"
      << "Last Fragment   : " << _last_frag
      ;
}

void TpcRceReceiver::_print_summary(const char *title) const
{
   auto curr = _receiver->get_stats();
   DAQLogger::LogInfo(_instance_name)
      << "=== " << title << " ===\n"
      << "NFrags   : " << curr.rx_cnt    << "\n"
      << "RSSI Drop: " << curr.rssi_drop << "\n"
      << "Pkt. Drop: " << curr.pack_drop << "\n"
      << "Overflow : " << curr.overflow  << "\n"
      << "Bad Hdrs : " << curr.bad_hdrs  << "\n"
      << "Bad Trlr : " << curr.bad_trlr  << "\n"
      << "Err Cnt  : " << curr.err_cnt 
      ;
}

void TpcRceReceiver::_send_stats() const
{
   auto last = artdaq::MetricMode::LastPoint;
   auto &mm = artdaq::Globals::metricMan_;

   mm->sendMetric("RceRecv EvtRate",   _stats.evt_rate,  "Hz",   1,  last);
   mm->sendMetric("RceRecv DataRate",  _stats.data_rate, "Gbps", 1,  last);
   mm->sendMetric("RceRecv AvgSize",   _stats.avg_size , "MB",   1,  last);
   mm->sendMetric("RceRecv MinSize",   _stats.min_size , "MB",   1,  last);
   mm->sendMetric("RceRecv MaxSize",   _stats.max_size , "MB",   1,  last);
   mm->sendMetric("RceRecv RSSIDrop",  _stats.rssi_drop, "",     1,  last);
   mm->sendMetric("RceRecv PackDrop",  _stats.pack_drop,  "",    1,  last);
   mm->sendMetric("RceRecv BadHdrs",   _stats.bad_hdrs,   "",    1,  last);
   mm->sendMetric("RceRecv ErrCnt",    _stats.err_cnt,    "",    1,  last);
   mm->sendMetric("RceRecv Overflow",  _stats.overflow,   "",    1,  last);
   mm->sendMetric("RceRecv IsOpen",    _stats.is_open,    "",    1,  last);
}

void TpcRceReceiver::_send_summary() const
{
   auto send = [](const char* name, uint32_t v)
   {
      artdaq::Globals::metricMan_->sendMetric(
            name, static_cast<long unsigned int>(v), "", 1,
            artdaq::MetricMode::LastPoint);
   };

   auto curr = _receiver->get_stats();
   send("RceRecv NFrags"       , curr.rx_cnt   );
   send("RceRecv TotalRSSIDrop", curr.rssi_drop);
   send("RceRecv TotalPackDrop", curr.pack_drop);
   send("RceRecv TotalOverflow", curr.overflow );
   send("RceRecv TotalBadHdrs" , curr.bad_hdrs );
   send("RceRecv TotalBadTrlr" , curr.bad_trlr );
   send("RceRecv TotalErrCnt"  , curr.err_cnt  );
}

void TpcRceReceiver::_check_status() 
{
   size_t n_retries  = 0;
   size_t max_trials = 5;
   do {
      ++n_retries;

      auto status = _rce_comm->get_status();
      bool pass = status.check_hls(_hls_mask);

      if (pass)
         break;
      else if (n_retries == max_trials) 
         DAQLogger::LogWarning(_instance_name) << status.err_msg();

   } while (n_retries < max_trials);
}

} //namespace dune
// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TpcRceReceiver)
