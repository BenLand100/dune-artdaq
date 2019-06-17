#ifndef METADATAMANAGER_H
#define METADATAMANAGER_H

#include "fhiclcpp/ParameterSet.h"

#include <map>
#include <string>
#include <vector>

namespace meta {

  class MetadataManager {
    
  public:
    static MetadataManager& getInstance(){
      static MetadataManager instance;
      return instance;
    }
    
    void AddMetadata(std::string key, std::string value);
    void AddMetadataHighPrio(std::string key, std::string value);
    void DoParsing(fhicl::ParameterSet const & pset);
    
    /// Determines if a key came from the parameter set or was inherited
    std::vector<std::string>& GetKeys() { return fKeys; }
    
    std::map< std::string, std::string >& GetMetadata();
    
  private:
    MetadataManager();
    MetadataManager(MetadataManager const&);
    void operator=(MetadataManager const&);
    
    /// Descends through parameter set constructing fully qualified keys
    void ParsePSet(std::string key, fhicl::ParameterSet const & pset);
    std::string CleanWhiteSpace(std::string str);
    std::string ToLower(std::string str);  
    
    std::vector<std::string> fKeys;
    std::map< std::string, std::string > fPSetMetaMap;
    std::map< std::string, std::string > fMetaMap;
    std::map< std::string, std::string > fHighPrioMetaMap;

  };
} // end namespace meta
#endif
///////////////////////////////////////////////////////////////////////

