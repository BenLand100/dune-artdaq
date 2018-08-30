#ifndef RCESTATUS_HH__
#define RCESTATUS_HH__

#include <string>
#include <sstream>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/optional.hpp>

namespace dune {
namespace rce {

class RceStatus
{
   typedef boost::property_tree::ptree PTree;
   public:
      RceStatus(const std::string& xml_string);

      void clear_err_msg() { _err_msg.str(""); }
      std::string err_msg() const { return _err_msg.str(); }

      const PTree& get_tree( const char *label="" ) const { 
         return _ptree.get_child(label); 
      };

      //void dump(const char *label="");

      bool check_hls(uint8_t mask=0);

   private:
      PTree             _ptree;
      std::stringstream _err_msg;

      void _dump_tree(const PTree &ptree, std::ostream& stream, int level=0);

      struct _Hex_to_Int
      {
         typedef std::string internal_type;
         typedef uint32_t    external_type;

         boost::optional<uint32_t> get_value(const std::string &s)
         {
            char *c;
            uint32_t l = std::strtoul(s.c_str(), &c, 16);
            return boost::make_optional(c != s.c_str(), l);
         }
      } _hex_to_int;
};
}}
#endif
