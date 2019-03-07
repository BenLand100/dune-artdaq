#ifndef dune_artdaq_Generators_HitFinder_PTMPConfig_hh
#define dune_artdaq_Generators_HitFinder_PTMPConfig_hh

#include "ptmp/api.h"

namespace dune {

  // A struct that handles the PTMP config for the receiver
  struct ReceiverPTMP_Config {
    std::string socktype;
    std::string attach;
    std::vector<std::string> endpoints;
    int timeout;
    ReceiverPTMP_Config(const std::string& SocketType,
                        const std::string& Attach,
                        const std::vector<std::string>& EndPoints,
                        const int& Timeout):
      socktype (SocketType),
      attach   (Attach),
      endpoints(EndPoints),
      timeout  (Timeout)
      {}

  };
  
  // A struct that handles the PTMP config for the sender
  struct SenderPTMP_Config {
    std::string type;
    std::string endpoint;
    SenderPTMP_Config(const std::string& t,
                      const std::string& e):
      type    (t),
      endpoint(e)
      {}
  };

}

#endif
