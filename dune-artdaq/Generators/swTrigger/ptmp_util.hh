#include "json.hpp"

#include "fhiclcpp/ParameterSet.h"

#include <sstream>

#include "ptmp/upif.h"

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
        root["socket"]["hwm"]=100000;
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
                                          uint64_t tbuf,
                                          std::string insocktype="SUB",
                                          std::string outsocktype="PUB")
    {
        json root;
        root["input"]=make_ptmp_socket_json(insocktype, "connect", inendpoints);
        root["output"]=make_ptmp_socket_json(outsocktype, "bind", outendpoints);
        root["tspan"]=tspan;
        root["tbuf"]=tbuf;
        return root.dump();
    }
    
    // Utility function to setup the TPSorted
    // tardy     - The time (millisec) which it will wait for a TPSet before marking it tardy 
    // (in/out)endpoints - A list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    std::string make_ptmp_tpsorted_string(std::vector<std::string> inendpoints,
                                          std::vector<std::string> outendpoints,
                                          int tardy,
                                          std::string insocktype="SUB",
                                          std::string outsocktype="PUB")
    {
        json root;
        root["input"]=make_ptmp_socket_json(insocktype, "connect", inendpoints);
        root["output"]=make_ptmp_socket_json(outsocktype, "bind", outendpoints);
        root["sync_time"]=tardy;
        return root.dump();
    }

    // Utility function to setup TPFilter, the nterface to the the trigger algorithms
    // (in/out)endpoints - A list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
    // method - The trigger candidate or decision algorithm name.
    // name - The thread name where TPFilter is running.
    std::string make_ptmp_tpfilter_string(std::vector<std::string> inendpoints,
                                          std::vector<std::string> outendpoints,
                                          std::string algorithm,
                                          std::string threadname,
                                          json* config=nullptr)
    {
        json root;
        root["input"]=make_ptmp_socket_json("SUB", "connect", inendpoints);
        root["output"]=make_ptmp_socket_json("PUB", "bind", outendpoints);
        root["engine"]=algorithm;
        root["name"]=threadname;
        if(config) root["engine_config"]=*config;
        return root.dump();
    }

    // Make a vector of endpoints which are specified in the parameter
    // set via two indirections: The key `key` has a string value
    // which is itself the name of a key in the "ptmp_connections"
    // table. *That* key has a value which is a list of key names
    // whose values are the actual endpoints we want. Worked out:
    //
    // fragment_receiver: {
    //     ptmp_connections: {
    //         felix501: "tcp://ip:port1"
    //         felix502: "tcp://ip:port2"
    //         # DAQInterface/RC set the following based on active components
    //         trigcand500_tpwindow_inputs_keys: ["felix501","felix502"]
    //     }
    //     tpwindow_input_connections_key: "trigcand500_tpwindow_inputs_keys"
    // 
    //
    // This horribly
    // complex scheme is the only way I (with help from Kurt) could
    // come up with to enable just the endpoints for the components
    // that are active in the partition, and have trigcand500 and
    // trigcand600 listen to different inputs.
    std::vector<std::string> endpoints_for_key(fhicl::ParameterSet const& ps,
                                               std::string key)
    {
        fhicl::ParameterSet const& conns_table=ps.get<fhicl::ParameterSet>("ptmp_connections");
        std::string val1=ps.get<std::string>(key);
        std::vector<std::string> vals=conns_table.get<std::vector<std::string>>(val1);
        std::vector<std::string> ret;
        for(auto const& val: vals){
            ret.push_back(conns_table.get<std::string>(val));
        }
        return ret;
    }

    bool add_plugin_libraries(const std::vector<std::string>& libs)
    {
        for(auto const& lib: libs){
            if(!upif::plugins().add(lib)){
                return false;
            }
        }
        return true;
    }

} //ptmp_util
