#include "Log.h"

std::ostream* SSPDAQ::Log::fErrorStream=&std::cerr;
std::ostream* SSPDAQ::Log::fWarningStream=&std::cerr;
std::ostream* SSPDAQ::Log::fInfoStream=&std::cout;
std::ostream* SSPDAQ::Log::fDebugStream=&std::cout;
std::ostream* SSPDAQ::Log::fTraceStream=&std::cout;

std::ostream* SSPDAQ::Log::junk=new std::ostream(0);
