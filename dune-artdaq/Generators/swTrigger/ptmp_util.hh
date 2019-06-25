#include "json/json.h"

namespace ptmp_util
{
// `pattern` is the socket type (PUB, SUB, etc)
// `bind_or_connect` is a literal "bind" or "connect" for which end you want
// `endpoints` is a list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
std::string make_ptmp_socket_string(std::string pattern,
                                    std::string bind_or_connect,
                                    std::vector<std::string> endpoints)
{
    Json::Value root(Json::objectValue);
    Json::Value endpoints_json(Json::arrayValue);
    for(auto const& ep: endpoints) endpoints_json.append(ep);
    root["socket"]=Json::Value(Json::objectValue);
    root["socket"]["type"]=pattern;
    root["socket"][bind_or_connect]=endpoints_json;
    std::stringstream ss;
    ss << root;
    return ss.str();
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
    Json::Value root(Json::objectValue);
    Json::Value inendpoints_json(Json::arrayValue);
    Json::Value outendpoints_json(Json::arrayValue);
    for(auto const& inep: inendpoints) inendpoints_json.append(inep);
    for(auto const& outep: outendpoints) outendpoints_json.append(outep);
    root["input"]["socket"]["type"]="SUB";
    root["input"]["socket"]["connect"]=inendpoints_json;
    root["output"]["socket"]["type"]="PUB";
    root["output"]["socket"]["bind"]=outendpoints_json;
    root["tspan"]=tspan;
    root["tbuffer"]=tbuf;
    std::stringstream ss;
    ss << root;
    return ss.str();
}

// Utility function to setup the TPSorted
// tardy     - The time (millisec) which it will wait for a TPSet before marking it tardy 
// (in/out)endpoints - A list of strings representing endpoints to bind/connect to, eg "tcp://*:12345"
std::string make_ptmp_tpsorted_string(std::vector<std::string> inendpoints,
                                    std::vector<std::string> outendpoints,
                                    int tardy)
{
    Json::Value root(Json::objectValue);
    Json::Value inendpoints_json(Json::arrayValue);
    Json::Value outendpoints_json(Json::arrayValue);
    for(auto const& inep: inendpoints) inendpoints_json.append(inep);
    for(auto const& outep: outendpoints) outendpoints_json.append(outep);
    root["input"]["socket"]["type"]="SUB";
    root["input"]["socket"]["connect"]=inendpoints_json;
    root["output"]["socket"]["type"]="PUB";
    root["output"]["socket"]["bind"]=outendpoints_json;
    root["tardy"]=tardy;
    std::stringstream ss;
    ss << root;
    return ss.str();
}

} //ptmp_util
