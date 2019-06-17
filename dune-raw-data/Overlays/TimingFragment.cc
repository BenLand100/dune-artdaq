#include "dune-raw-data/Overlays/TimingFragment.hh"

std::ostream & dune::operator << (std::ostream & os, TimingFragment const & f) {
  os << "TimingFragment size=" << std::dec << f.size()
     << ", cookie 0x"          << std::hex << f.get_cookie()
     << ", scmd 0x"            << std::hex << f.get_scmd()
     << ", tcmd 0x"            << std::hex << f.get_tcmd()
     << ", timestamp 0x"       << std::hex << f.get_tstamp()
     << ", evtctr="            << std::dec << f.get_evtctr()
     << ", cksum 0x"           << std::hex << f.get_cksum()
     << ", last_run_start 0x"  << std::hex << f.get_last_runstart_timestamp()
     << ", last_spill_start 0x"<< std::hex << f.get_last_spillstart_timestamp()
     << ", last_spill_end 0x"  << std::hex << f.get_last_spillend_timestamp()

     << std::dec << "\n";

  return os;
}
