#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <iostream>
#include <memory>
#include <iomanip>

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  std::string wib_address = ps.get<std::string>("WIB.address");
  dune::DAQLogger::LogInfo("WIBReader") << "Connecting to WIB at " <<  wib_address;

  // JCF, Sep-14-2017

  // It's safer to use one of C++11's "smart" pointers than to use a
  // raw pointer, as objects pointed to by smart pointers will be
  // automatically cleaned up when the smart pointer goes out of
  // scope, without requiring the programmer to cleanup with the
  // "delete" keyword. However, be aware that here, deallocation will
  // occur at the end of the constructor. To make the WIB instance
  // persistent through the lifetime of WIBReader, make it a WIBReader
  // member.

  auto wib = std::make_unique<WIB>( wib_address, "WIB.adt", "FEMB.adt" );

  fhicl::ParameterSet wib_config = ps.get<fhicl::ParameterSet>("WIB.config");
  // Read values from FHiCL document and feed them to WIB library
  for (std::string key : wib_config.get_names()) {
    if (wib_config.is_key_to_atom(key)) {
      uint32_t value = wib_config.get<long int>(key);
      wib->Write(key, value);
      dune::DAQLogger::LogInfo("WIBReader") << "Set " << key << " to 0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec;
    }
  }
  dune::DAQLogger::LogInfo("WIBReader") << "Configured WIB";
}

// "shutdown" transition
WIBReader::~WIBReader() {

}

// "start" transition
void WIBReader::start() {

}

// "stop" transition
void WIBReader::stop() {

}

// Called by BoardReaderMain in a loop between "start" and "stop"
bool WIBReader::getNext_(artdaq::FragmentPtrs& /*frags*/) {
  return false;
}

} // namespace

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(wibdaq::WIBReader)
