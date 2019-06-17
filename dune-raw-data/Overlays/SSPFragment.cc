#include "dune-raw-data/Overlays/SSPFragment.hh"

#include "cetlib/exception.h"

namespace {

  // JCF, 9/29/14

  // pop_count() is currently unused; under the new compiler (gcc
  // 4.9.1 as opposed to 4.8.2) this registers as an error
  // (-Werror=unused-function) so I've commented it out for now


  // unsigned int pop_count (unsigned int n) {
  //   unsigned int c; 
  //   for (c = 0; n; c++) n &= n - 1; 
  //   return c;
  // }
}

std::ostream & dune::operator << (std::ostream & os, SSPFragment const & f) {
  os << "SSPFragment event size: "
     << f.hdr_event_size()
     << ", run number: "
     << f.hdr_run_number()
     << "\n";

  return os;
}

