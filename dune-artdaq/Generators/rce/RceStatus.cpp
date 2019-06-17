#include <iostream>

#include "RceStatus.hh"

namespace dune {
namespace rce {

RceStatus::RceStatus(const std::string& xml_string)
{
   std::stringstream ss;
   ss << xml_string;
   
   boost::property_tree::xml_parser::read_xml(ss, _ptree);
}

void RceStatus::_dump_tree(const PTree &ptree, std::ostream& stream, int level)
{
   if (ptree.empty()) {
      stream << " : " << ptree.data();
   }
   else {
      for ( auto const& node : ptree) {
         stream << std::setw(level) << " +" << node.first;

         _dump_tree(node.second, stream, level + 1);
         stream << '\n';
      }
   }
}

bool RceStatus::check_hls(uint8_t mask)
{
   const auto& hls = _ptree.get_child("system.status.DataDpm.DataDpmHlsMon");
   auto get = [&](char const *label)
   {
      return hls.get<uint32_t>(label, _hex_to_int);
   };

   bool pass = true;

   // check 1st stream
   if ((mask & 0x1) == 0 && 
         ( get("IbBandwidth0") == 0 ||
           get("ObBandwidth0") == 0 || 
           get("IbFrameRate0") == 0 || 
           get("ObFrameRate0") == 0) ) {
      pass = false;
      _err_msg << "HLS-0 Lock-up\n";
   }

   // check 2nd stream
   if ((mask & 0x2) == 0 && 
         ( get("IbBandwidth1") == 0 ||
           get("ObBandwidth1") == 0 || 
           get("IbFrameRate1") == 0 || 
           get("ObFrameRate1") == 0)) {
      pass = false;
      _err_msg << "HLS-1 Lock-up\n";
   }

   if (!pass) _dump_tree(hls, _err_msg);

   return pass;
}

}}
