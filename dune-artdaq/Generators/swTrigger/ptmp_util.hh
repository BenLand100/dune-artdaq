#include "json.hpp"

namespace ptmp_util
{
    // `pattern` is the socket type (PUB, SUB, etc)
    // `bind_or_connect` is a literal "bind" or "connect" for which end you want
    // `endpoints` is a list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    nlohmann::json make_ptmp_socket_json(std::string pattern,
                                      std::string bind_or_connect,
                                      std::vector<std::string> endpoints)
    {
        using json = nlohmann::json;
        json root;
        root["socket"]["type"]=pattern;
        for(auto const& ep: endpoints){
            root["socket"][bind_or_connect].push_back(ep);
        }
        return root;
    }

    // `pattern` is the socket type (PUB, SUB, etc)
    // `bind_or_connect` is a literal "bind" or "connect" for which end you want
    // `endpoints` is a list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    std::string make_ptmp_socket_string(std::string pattern,
                                        std::string bind_or_connect,
                                        std::vector<std::string> endpoints)
    {
        return make_ptmp_socket_json(pattern, bind_or_connect, endpoints).dump();
    }
    
}
