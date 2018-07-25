#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "artdaq/DAQdata/Globals.hh"
#include "dune-artdaq/Generators/TpcRceReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"
//#include "canvas/Utilities/Exception.h"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
//#include "cetlib/exception.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

namespace dune {

rce::BRStats::BRStats() :
   last_update(boost::posix_time::second_clock::local_time()),
   last_frag(""),
   bad_hdrs(0),
   nfrags(0)
{
}

TpcRceReceiver::TpcRceReceiver(fhicl::ParameterSet const& ps) :
   CommandableFragmentGenerator(ps)
{

   typedef std::string str;
   _rce_host_addr = ps.get<str>  ("rce_client_host_addr"                );
   //_rce_host_port = ps.get<str>  ("rce_client_host_addr"  , "8090"      );
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
   //

   _rce_comm.reset(new rce::RceComm      (_rce_host_addr       ));
   _receiver.reset(new rce::RssiReceiver (_rce_host_addr, 8192 ));
}

void TpcRceReceiver::start(void) 
{
   // config rce
   _rce_comm->reset         ( _rce_xml_file  );
   _rce_comm->set_run_mode  ( _rce_run_mode  );
   _rce_comm->set_emul      ( _rce_feb_emul  );
   _rce_comm->set_partition ( _partition     );
   _rce_comm->enable_rssi   ( _enable_rssi   );
   _rce_comm->set_daq_host  ( _daq_host_addr );

   // validate config

   // connect receiver
   
   // drain buffer

   // send start command
   _rce_comm->send_cmd("SetRunState", "Enable");
}

void TpcRceReceiver::stop(void)
{
   _rce_comm->send_cmd("SetRunState", "Stopped");
   DAQLogger::LogInfo(_instance_name) << "stop()\n";
   _print_stats();
}

void TpcRceReceiver::stopNoMutex(void)
{
   _rce_comm->send_cmd("SetRunState", "Stopped");
   DAQLogger::LogInfo(_instance_name) << "stopNoMutex()\n";
   _print_stats();
}

bool TpcRceReceiver::getNext_(artdaq::FragmentPtrs& frags)
{
   dune::rce::BufferPtr buf;
   if (_receiver->pop(buf))
   {
      // an unique pointer, take ownership of buf
      artdaq::FragmentPtr frag(buf);

      //auto *data_ptr = reinterpret_cast<uint64_t*>(buf->dataAddress());
      // FIXME
      auto *data_ptr = reinterpret_cast<uint64_t*>(buf->dataBeginBytes() + 12);

      auto *header = data_ptr;
      auto timestamp = *(header + 2);
      if (*header >> 40 != 0x8b309e)
         ++_stats.bad_hdrs;

      // Set fragment fields appropriately
      frag->setSequenceID ( ev_counter()      );
      frag->setFragmentID ( _frag_id          );
      frag->setUserType   ( dune::detail::TPC );
      frag->setTimestamp  ( timestamp         );

      // debug info
      //TLOG(50, "RCE")
      _debug_ss.str("");
      _debug_ss
         << "[" << _instance_name << "] "
         << "size:"      << frag->dataSizeBytes() << ", "
         << "frag id:"   << frag->fragmentID()    << ", "
         << "seq id:"    << frag->sequenceID()      << ", "
         << std::hex
         << "timestamp:" << frag->timestamp()     << ", "
         << "header:"    << *header
         << std::dec;

      DAQLogger::LogInfo(_instance_name) << _debug_ss.str();

      // add the fragment to the list
      frags.emplace_back(std::move(frag));

      // increment the event counter
      ev_counter_inc();

      // update BoardReader stats
      _stats.last_frag = _debug_ss.str();
      ++_stats.nfrags;
   }
   else {
      boost::this_thread::sleep(boost::posix_time::milliseconds(50));
   }

   // print stats
   auto now = boost::posix_time::second_clock::local_time();
   auto dt  = now - _stats.last_update;
   if (dt.total_seconds() > 1) {
      _stats.last_update = now;
      _print_stats();

      _stats_prev = _stats;
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

void TpcRceReceiver::_print_stats()
{
   auto recv_stat = _receiver->get_stats();
   auto last_update = to_simple_string(_stats.last_update);

   auto timediff = _stats.last_update  - _stats_prev.last_update;
   float dt = timediff.total_milliseconds() * 1e-3;
   float frag_rate = (_stats.nfrags - _stats_prev.nfrags) / dt;

   DAQLogger::LogInfo(_instance_name)
      << "[" << _instance_name << "] "
      << "Last Update     : " << last_update            << "\n"
      << "Rate (Hz)       : " << frag_rate              << "\n"
      << "Fragments Sent  : " << _stats.nfrags          << "\n"
      << "Bad Headers     : " << _stats.bad_hdrs        << "\n"
      << "RSSI Drop       : " << recv_stat.rssi_drop    << "\n"
      << "Pkt. Drop       : " << recv_stat.pack_drop    << "\n"
      << "Buffer Overflow : " << recv_stat.buf_overflow << "\n"
      << "Last Fragment   : " << _stats.last_frag;
}

} //namespace dune
// The following macro is defined in artdaq's GeneratorMacros.hh header
DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(dune::TpcRceReceiver)
