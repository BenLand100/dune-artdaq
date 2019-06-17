#include "dune-raw-data/Overlays/TriggerFragment.hh"

std::ostream & dune::operator << (std::ostream & os, TriggerFragment const & f) {
  os << "TriggerFragment size=" << std::dec << f.size()
     << ", cookie 0x"          << std::hex << f.get_cookie()
     << ", scmd 0x"            << std::hex << f.get_scmd()
     << ", tcmd 0x"            << std::hex << f.get_tcmd()
     << ", timestamp 0x"       << std::hex << f.get_tstamp()
     << ", evtctr="            << std::dec << f.get_evtctr()
     << ", cksum 0x"           << std::hex << f.get_cksum()
     << ", last_run_start 0x"  << std::hex << f.get_last_runstart_timestamp()
     << ", last_spill_start 0x"<< std::hex << f.get_last_spillstart_timestamp()
     << ", last_spill_end 0x"  << std::hex << f.get_last_spillend_timestamp()
     << ", count 0x"           << std::hex << f.get_count()
     << ", partid 0x"          << std::hex << f.get_partid()
     << ", sources 0x"         << std::hex << f.get_sources()
     << ", tstart 0x"          << std::hex << f.get_tstart()
     << ", tspan 0x"           << std::hex << f.get_tspan()
     << ", modid 0x"           << std::hex << f.get_modid()
     << ", submodids 0x"       << std::hex << f.get_submodids()

     << std::dec << "\n";

  return os;
}
