#include "dune-artdaq/Metadata/MetadataManager.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iterator>
#include <memory>
#include <sstream>

#include "TString.h"

namespace meta {
  
  //.................................................................
  MetadataManager::MetadataManager() 
  {
    fKeys.clear();
  }
  
  //..................................................................
  std::map< std::string, std::string >& MetadataManager::GetMetadata() 
  {
    //update metadata with information from parsing the pset
    for(auto& it: fPSetMetaMap){
      std::string key = "dunemeta." + it.first;
      fMetaMap[key] = it.second;
    }
    //update metadata with information from the high priority map
    for(auto& it: fHighPrioMetaMap){
      std::string key = "dunemeta." + it.first;
      fMetaMap[key] = it.second;
    }
    return fMetaMap;
  }

  //..................................................................
  std::string MetadataManager::ToLower(std::string str)
  {
    std::transform(str.begin(), 
                   str.end(), 
                   str.begin(), 
                   (int(*)(int))std::tolower );
    return str;
  }
  
  //..................................................................
  std::string MetadataManager::CleanWhiteSpace(std::string str)
  {
    std::string out;
    std::copy_if(str.begin(), 
                 str.end(), 
                 std::back_inserter(out), 
                 [](char c){ return !(std::isspace(c));});
    return out;
  }
  
  //......................................................................
  void MetadataManager::ParsePSet(std::string key, 
                                  fhicl::ParameterSet const & inPSet )
  {
    fhicl::ParameterSet pset;
    if(key == "") pset = inPSet;
    else pset = inPSet.get<fhicl::ParameterSet>(key);
    
    std::vector<std::string> pset_keys = pset.get_pset_names();
    std::vector<std::string> all_keys  = pset.get_names();
    std::vector<std::string> nonpset_keys;
    
    std::sort(pset_keys.begin(), pset_keys.end());
    std::sort(all_keys.begin(), all_keys.end());
    
    // get keys that are in all, but not in pset 
    std::set_difference(all_keys.begin(), all_keys.end(),
                        pset_keys.begin(), pset_keys.end(),
                        std::inserter( nonpset_keys, nonpset_keys.begin())
                        );
  
    //all non-pset keys are ready to be saved in the vector of keys
    for(auto& it: nonpset_keys){
      std::string newkey;
      if(key == "") newkey = it;
      else newkey = key + (std::string)"." + it;
      fKeys.push_back(newkey);
    }
    //all pset keys should be made into fully qualified keys and 
    //put into a recursive call
    for(auto& it: pset_keys){
      std::string newkey;
      if(key == "") newkey = it;
      else newkey = key + (std::string)"." + it;
      ParsePSet( newkey, inPSet );
    }
    //when it reaches here, it's made it through the pset_key loop and is done
    return;
  }

  //..................................................................
  void MetadataManager::DoParsing(fhicl::ParameterSet const & inPSet)
  {
    //first, clear fKeys to reset the parsing
    fKeys.clear();
    
    ParsePSet("", inPSet);
    //loop through keys, find values & fill metadata map 
    
    for(auto& it: fKeys){
      //get value from parameter set
      const std::string key = CleanWhiteSpace(ToLower(it));
      
      // It's always possible to represent the key value as a string, but...
      std::string value = CleanWhiteSpace(inPSet.get<std::string>(it));

      // See https://cdcvs.fnal.gov/redmine/issues/7450

      // Large ints get expressed in floating point notation, which isn't what
      // we want. If the output is numeric, detect that. strtod returns zero
      // for failure and for actual zeroes, but actual zeroes will have good
      // string representations so we're OK.
      char* endptr;
      if(strtod(value.c_str(), &endptr) != 0){
        // Make sure we used all the characters
        if(endptr == value.c_str()+value.size()){
          // We know there are no actual floating point numbers in the
          // metadata, so it's safe to fetch as an int and format the way we
          // want.
          value = TString::Format("%ld", inPSet.get<long>(it));
        }
      }

      fPSetMetaMap[ key ] = value;
    }
  }
  
  //..............................................................
  void MetadataManager::AddMetadata(std::string key, std::string value)
  {
    key = ToLower(key);
    key = CleanWhiteSpace(key);
    key = "dunemeta." + key;

    value = CleanWhiteSpace(value);
    
    fMetaMap[ key ] = value;
  }


  //..............................................................
  void MetadataManager::AddMetadataHighPrio(std::string key, std::string value)
  {
    key = ToLower(key);
    key = CleanWhiteSpace(key);
    key = "dunemeta." + value;

    value = CleanWhiteSpace(value);
    
    fHighPrioMetaMap[ key ] = value;
  }
  
} // end namespace meta

