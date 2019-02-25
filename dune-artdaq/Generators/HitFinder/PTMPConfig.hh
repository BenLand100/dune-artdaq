#ifndef dune_artdaq_Generators_HitFinder_PTMPConfig_hh
#define dune_artdaq_Generators_HitFinder_PTMPConfig_hh

#include "ptmp/json.hpp"

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

    // To initiate the PTMPReceiver, pass in the config
    std::string DumpJSON() {
      json jcfg
        jcfg["socket"]["type"] = socktype;
      for (const auto& endpoint : endpoints) {
        jcfg["socket"][attach].push_back(endpoint);
      }
      return jcfg.dump();
    }
  };
  
  // A struct that handles the PTMP config for the sender
  struct SenderPTMP_Config {
    std::string type;
    std::string endpoint;
    SenderPTMP_Config(const std::string& type,
                      const std::string& endpoint):
      type    (Type),
      endpoint(EndPoint)
      {}
    
    // To initiate the PTMPReceiver, pass in the config
    std::string DumpJSON() {
      json jcfg;
      jcfg["socket"]["type"] = argv[2];
      jcfg["socket"][argv[3]][0] = argv[4];
      return jcfg.dump();
    }
  };

}


#endif
