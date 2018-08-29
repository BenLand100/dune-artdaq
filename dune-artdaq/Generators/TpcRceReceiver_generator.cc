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

   _daq_host_addr = boost::asio::ip::host_name();

   int board_id = ps.get<int>("board_id", 0);

   std::stringstream ss;
   ss << "COB" << board_id / 8 + 1
      << "-RCE" << board_id % 8 + 1;
   _instance_name = ss.str();

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
   }
   catch (const std::exception &e) {
      DAQLogger::LogError(_instance_name) << e.what();
   }
}

void TpcRceReceiver::start(void) 
{
   // validate config

   // connect receiver
   _receiver.reset(new rce::RssiReceiver (_rce_host_addr, 8192));
  
   // drain buffer

   // send start command
   _rce_comm->blowoff_wib( false );
   _rce_comm->blowoff_hls( 0x0   );

   _rce_comm->send_cmd("SetRunState", "Enable");
}

void TpcRceReceiver::stop(void)
{
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
   _print_summary("End of run summary");
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

   // update / send stats every second
   auto now = boost::posix_time::second_clock::local_time();
   auto dt  = now - _stats.last_update;
   auto lapse = dt.total_seconds();
   if (lapse > 1) {
      _update_stats();
      _send_stats();
   }

   // debug
   if (lapse > 0 && _frag_cnt % 10 == 1)
      _print_stats();

   if (lapse > 0 && _frag_cnt % 100 == 1)
      _print_summary("Cum. Stats");

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

   auto rx_count = curr.rx_count - _stats.prev.rx_count;
   auto rx_bytes = curr.rx_bytes - _stats.prev.rx_bytes;

   _stats.evt_rate  = lapse > 0    ? rx_count / lapse : 0;
   _stats.data_rate = lapse > 0    ? rx_bytes / lapse * 8e-9 : 0;
   _stats.avg_size  = rx_count > 0 ? rx_bytes / rx_count * 1e-6 : 0;
   _stats.rssi_drop = curr.rssi_drop - _stats.prev.rssi_drop;
   _stats.pack_drop = curr.pack_drop - _stats.prev.pack_drop;
   _stats.bad_hdrs  = curr.bad_hdrs  - _stats.prev.bad_hdrs;
   _stats.bad_data  = curr.bad_data  - _stats.prev.bad_data;
   _stats.overflow  = curr.overflow  - _stats.prev.overflow;
   _stats.is_open   = _receiver->is_open();

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
      << "Bad Data        : " << _stats.bad_hdrs   << "\n"
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
      << "NFrags : "   << curr.rx_count  << "\n"
      << "RSSI Drop: " << curr.rssi_drop << "\n"
      << "Pkt. Drop: " << curr.pack_drop << "\n"
      << "Overflow : " << curr.overflow  << "\n"
      << "Bad Hdrs : " << curr.bad_hdrs  << "\n"
      << "Bad Trlr:  " << curr.bad_trlr  << "\n"
      << "Bad Data : " << curr.bad_data 
      ;
}

void TpcRceReceiver::_send_stats() const
{
}

} //namespace dune
// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TpcRceReceiver)
