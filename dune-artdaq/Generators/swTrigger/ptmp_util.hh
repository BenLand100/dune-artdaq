#include "json.hpp"

#include <sstream>

namespace ptmp_util
{

  using json = nlohmann::json;

    // `pattern` is the socket type (PUB, SUB, etc)
    // `bind_or_connect` is a literal "bind" or "connect" for which end you want
    // `endpoints` is a list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    nlohmann::json make_ptmp_socket_json(std::string pattern,
                                      std::string bind_or_connect,
                                      std::vector<std::string> endpoints)
    {
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

    // Utility function to setup the TPWindow 
    // tspan     - The width of the window (in 50MHz ticks) which the TPs are windowed 
    // tbuf      - The width of the TP buffer in 50MHz ticks
    // (in/out)endpoints - A list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    std::string make_ptmp_tpwindow_string(std::vector<std::string> inendpoints,
                                         std::vector<std::string> outendpoints,
                                         uint64_t tspan,
                                         uint64_t tbuf)
    {
        json root;
        root["input"]=make_ptmp_socket_json("SUB", "connect", inendpoints);
        root["output"]=make_ptmp_socket_json("PUB", "bind", outendpoints);
        root["tspan"]=tspan;
        root["tbuffer"]=tbuf;
        return root.dump();
    }
    
    // Utility function to setup the TPSorted
    // tardy     - The time (millisec) which it will wait for a TPSet before marking it tardy 
    // (in/out)endpoints - A list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    std::string make_ptmp_tpsorted_string(std::vector<std::string> inendpoints,
                                        std::vector<std::string> outendpoints,
                                        int tardy)
    {
        json root;
        root["input"]=make_ptmp_socket_json("SUB", "connect", inendpoints);
        root["output"]=make_ptmp_socket_json("PUB", "bind", outendpoints);
        root["tardy"]=tardy;
        return root.dump();
    }
    
    
} //ptmp_util
