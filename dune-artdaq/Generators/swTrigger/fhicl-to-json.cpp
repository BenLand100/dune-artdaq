// fhicl-to-json.cpp -
//
// Philip Rodrigues
// Friday, August  9 2019
//

#include "fhicl-to-json.hh"

#include <fhiclcpp/make_ParameterSet.h>

namespace fhicl_to_json
{

//======================================================================
std::string Jsonifier::jsonify(const fhicl::ParameterSet& ps)
{
    m_json.clear();
    while(!m_json_stack.empty()) m_json_stack.pop();
    m_json_stack.push(m_json);
    ps.walk(*this);
    return m_json.dump();
}

//======================================================================
std::string Jsonifier::jsonifyFile(std::string filename)
{
    fhicl::ParameterSet ps;
    cet::filepath_maker fm;
    make_ParameterSet(filename,
                      fm,
                      ps);
    return jsonify(ps);
}

//======================================================================
std::string Jsonifier::jsonifyString(std::string input)
{
    fhicl::ParameterSet ps;
    make_ParameterSet(input,
                      ps);
    return jsonify(ps);
}

//======================================================================
void Jsonifier::enter_table(key_t const& k, any_t const&) 
{
    m_json_stack.top().get()[k]=nlohmann::json::object();
    m_json_stack.push(m_json_stack.top().get()[k]);
}

//======================================================================
void Jsonifier::enter_sequence(key_t const& k, any_t const&) 
{
    m_json_stack.top().get()[k]=nlohmann::json::array();
    m_json_stack.push(m_json_stack.top().get()[k]);
}

//======================================================================
void Jsonifier::exit_table(key_t const&, any_t const&) 
{
    m_json_stack.pop();
}

//======================================================================
void Jsonifier::exit_sequence(key_t const&, any_t const&) 
{
    m_json_stack.pop();
}

//======================================================================
void Jsonifier::atom(key_t const& k, any_t const& a) 
{
    std::string strval=boost::any_cast<std::string>(a);
    nlohmann::json& cur=m_json_stack.top().get();
    if(cur.is_object()) cur[k]=nlohmann::json::parse(strval);
    if(cur.is_array()) cur.push_back(nlohmann::json::parse(strval));
}

}
