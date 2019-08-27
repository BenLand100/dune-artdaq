#ifndef FHICL_TO_JSON_HH
#define FHICL_TO_JSON_HH

#include "json.hpp"


#include <fhiclcpp/ParameterSet.h>
#include <fhiclcpp/ParameterSetWalker.h>

#include <stack>

namespace fhicl_to_json
{

class Jsonifier : public fhicl::ParameterSetWalker
{
public:
    Jsonifier() {}
    
    // Convert a fhicl::ParameterSet to json and return the string
    std::string jsonify(const fhicl::ParameterSet& ps);

    // Open fcl file `filename` and convert it to json string
    std::string jsonifyFile(std::string filename);

    // Convert fcl string `input` to json
    std::string jsonifyString(std::string input);

private:
    // Overrides from ParameterSetWalker that get called by walk()
    void enter_table(key_t const& k, any_t const& a) override;
    void enter_sequence(key_t const& k, any_t const& a) override;
    void exit_table(key_t const& k, any_t const& a) override;
    void exit_sequence(key_t const& k, any_t const& a) override;
    void atom(key_t const& k, any_t const& a) override;

    // The output json object
    nlohmann::json m_json;
    // The stack of json objects in the output. We need this so we can
    // keep track of the current object, and whether it's an object or
    // an array
    std::stack<std::reference_wrapper<nlohmann::json>> m_json_stack;
};

    std::string jsonify(const fhicl::ParameterSet& ps)
    {
        Jsonifier js;
        return js.jsonify(ps);
    }
}
#endif
