// JCF, Apr-2-2015

// The following is my repurposing of IceCube code sent to me by Erik
// Blaufuss; it's designed to make it easy to turn std::map-s (and
// other types of C++ object) into JSON code

/**
 * copyright  (C) 2004
 * the icecube collaboration
 * $Id: I3JSON.cxx 7629 2005-05-15 15:35:25Z tschmidt $
 *
 * @file I3JSON.cxx
 * @version $Revision:  $
 * @date $Date: 2005-05-15 17:35:25 +0200 (Sun, 15 May 2005) $
 * @author tschmidt
 */

// class header file
#include "I3JSON.hh"

#include <cmath>
#include <sstream>
#include <iomanip>

#include "cetlib/exception.h"

using namespace std;


namespace lbne
{
  string stringToJSON(const string& s)
  {
    string retVal("\"");
    for(string::const_iterator iter = s.begin(); iter != s.end(); ++iter)
      switch(*iter)
      {
        case '\n': retVal += "\\n"; break;
        case '\r': retVal += "\\r"; break;
        case '\t': retVal += "\\t"; break;
        case '\"': retVal += "\\\""; break;
        case '\\': retVal += "\\\\"; break;
        case '/' : retVal += "\\/"; break;
        case '\b': retVal += "\\b"; break;
        case '\f': retVal += "\\f"; break;
        default:
        // TODO: do UTF8 encoding into \uXXXX
        retVal += *iter;
      }
    return(retVal + "\"");
  }
  
  
  struct toJSON : public boost::static_visitor<string>
  {
    string operator()(null_t ) const
    {
      ostringstream oss;
      oss << "null";
      return(oss.str());
    }
    string operator()(boolean_t b) const
    {
      ostringstream oss;
      oss << (b ? "true" : "false");
      return(oss.str());
    }
    string operator()(const string& s) const
    {
      ostringstream oss;
      oss << stringToJSON(s);
      return(oss.str());
    }
    string operator()(const array_t& a) const
    {
      ostringstream oss;
      oss << "[";
      for(array_t::const_iterator iter = a.begin(); iter != a.end(); ++iter)
        oss << (iter == a.begin() ? "" : ", " ) << *iter;
      return(oss.str() + "]");
    }
    string operator()(const object_t& o) const
    {
      ostringstream oss;
      oss << "{";
      for(object_t::const_iterator iter = o.begin(); iter != o.end(); ++iter)
        oss << (iter == o.begin() ? "" : ", " ) << stringToJSON(iter->first)
            << " : " << iter->second;
      return(oss.str() + "}");
    }
    string operator()(double d) const
    {
      if(std::isnan(d) || std::isinf(d))
              throw cet::exception("invalid argument - "
      			     "JSON does not specify special floating point values, "
      			     "such as infinite or NaN");
      
      ostringstream oss;
      oss << scientific << setprecision(15);
      oss << d;
      return(oss.str());
    }
    template<class T>
    string operator()(T t) const
    {
      ostringstream oss;
      oss << t;
      return(oss.str());
    }
  };

ostream& operator<<(ostream& os, const value_t& value)
{
  os << boost::apply_visitor(toJSON(), value);
  
  return(os);
}

}

