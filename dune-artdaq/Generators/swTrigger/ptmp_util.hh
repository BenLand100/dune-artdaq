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

}
