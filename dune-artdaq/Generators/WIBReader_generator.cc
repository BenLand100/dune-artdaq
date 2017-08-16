#include "WIBReader.hh"
#include "artdaq/Application/GeneratorMacros.hh"

#include <iostream>

namespace wibdaq {

// "initialize" transition
WIBReader::WIBReader(fhicl::ParameterSet const& ps) :
    CommandableFragmentGenerator(ps) {

  std::string wib_address = ps.get<std::string>("WIB.address");
  printf("Connecting to WIB at %s\n", wib_address.c_str());

  wib = new WIB(wib_address, "WIB.adt", "FEMB.adt");

  fhicl::ParameterSet wib_config = ps.get<fhicl::ParameterSet>("WIB.config");
  // Read values from FHiCL document and feed them to WIB library
  for (std::string key : wib_config.get_names()) {
    if (wib_config.is_key_to_atom(key)) {
      uint32_t value = wib_config.get<long int>(key);
      wib->Write(key, value);
      printf("Set %s to 0x%08x\n", key.c_str(), value);
    }
  }
  printf("Configured WIB\n");
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
