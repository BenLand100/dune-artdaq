#include "dune-raw-data/Overlays/ToyFragment.hh"

#include "cetlib/exception.h"

namespace {

  // JCF, 9/29/14                                                                                                    
  // pop_count() is currently unused; under the new compiler (gcc                                                     // 4.9.1 as opposed to 4.8.2) this registers as an error                                                            // (-Werror=unused-function) so I've commented it out for now                                                     

  // unsigned int pop_count (unsigned int n) {
  //   unsigned int c; 
  //   for (c = 0; n; c++) n &= n - 1; 
  //   return c;
  // }
}

void dune::ToyFragment::checkADCData(int daq_adc_bits) const {
  dune::ToyFragment::adc_t const * adcPtr(findBadADC(daq_adc_bits));
  if (adcPtr != dataEnd()) {
    throw cet::exception("IllegalADCVal")
        << "Illegal value of ADC word #"
        << (adcPtr - dataBegin())
        << ": 0x"
        << std::hex
        << *adcPtr
        << ".";
  }
}

std::ostream & dune::operator << (std::ostream & os, ToyFragment const & f) {
  os << "ToyFragment event size: "
     << f.hdr_event_size()
     << ", run number: "
     << f.hdr_run_number()
     << "\n";

  return os;
}

