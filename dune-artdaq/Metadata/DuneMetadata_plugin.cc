////////////////////////////////////////////////////////////////////////
// Class:       DUNEMetadata
// Plugin Type: filecatalogmetadataplugin (art v2_08_04)
// File:        DUNEMetadata_plugin.cc
//
// Generated at Fri Jan 19 15:37:11 2018 by Jeremy Hewes using cetskelgen
// from cetlib version v3_01_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/FileCatalogMetadataPlugin.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "dune-artdaq/Metadata/MetadataManager.h"

#include <sstream>

namespace meta {

  class DUNEMetadata;

  class DUNEMetadata : public art::FileCatalogMetadataPlugin {
  public:
    using collection_type = std::vector<std::pair<std::string, std::string>>;

    explicit DUNEMetadata(fhicl::ParameterSet const & p);

    DUNEMetadata(DUNEMetadata const &) = delete;
    DUNEMetadata(DUNEMetadata &&) = delete;
    DUNEMetadata & operator = (DUNEMetadata const &) = delete;
    DUNEMetadata & operator = (DUNEMetadata &&) = delete;

    auto produceMetadata() -> collection_type override;
    void fillMetadata();

    void beginJob();
    void collectMetadata(art::Event const & e);
    void beginSubRun(art::SubRun const & sr);

  private:

    bool        fParsedHWConfig; // Has the module parsed hardware configuration information yet?
    bool        fAllowGeneric; // Do we allow generic metadata to be added?

    std::string fRunType;   // Run type

    int         fNEvts;
    int         fFirstEvent;
    int         fLastEvent;

    std::set<std::pair<int,int>> fSubRuns;

    bool GetExistingMetadata(std::string key, std::string & val);
    std::string GetOneInstance (std::string config_name, std::string key);
    std::vector<std::string> GetAllInstances(std::string config_dump, std::string key);

    std::vector<double> fEventList;

  }; // class DUNEMetadata


  DUNEMetadata::DUNEMetadata(fhicl::ParameterSet const & p)
    :
    FileCatalogMetadataPlugin(p),

    fParsedHWConfig(false),

    fAllowGeneric(p.get< bool >       ("allow_generic"  , false     )),
    fRunType     (p.get< std::string >("run_type"       ,"undefined")),

    fNEvts       (0),
    fFirstEvent  (-1),
    fLastEvent   (-1)

  {
    MetadataManager & manager = MetadataManager::getInstance();

    fhicl::ParameterSet inPSet;
    bool foundPSet = p.get_if_present<fhicl::ParameterSet>("params",inPSet);
    if (foundPSet && fAllowGeneric) manager.DoParsing(inPSet);

  } // function DUNEMetadata::DUNEMetadata

  auto DUNEMetadata::produceMetadata() -> collection_type
  {
    MetadataManager & manager = MetadataManager::getInstance();

    fillMetadata();

    auto md = manager.GetMetadata();
    std::vector<std::pair<std::string,std::string>> v;
    v.reserve(md.size());
    for (auto const& it : md) {
      v.push_back(std::make_pair(it.first,it.second));
    }

    fFirstEvent = -1;
    fNEvts = 0;
    fEventList.clear();

    return v;

  } // function DUNEMetadata::produceMetadata

  void DUNEMetadata::fillMetadata()
  {
    MetadataManager & manager = MetadataManager::getInstance();

    // Initialise output value stream
    std::ostringstream value_stream;

    // Event numbers metadata
    value_stream << fNEvts;
    manager.AddMetadata("event_count",value_stream.str());
    value_stream.str("");

    value_stream << fFirstEvent;
    manager.AddMetadata("first_event",value_stream.str());
    value_stream.str("");

    value_stream << fLastEvent;
    manager.AddMetadata("last_event",value_stream.str());
    value_stream.str("");

    // Add list of subruns
    value_stream << "[ ";
    for (auto it = fSubRuns.begin(); it != fSubRuns.end(); ++it) {
      if (it != fSubRuns.begin()) value_stream << ",\n";
      value_stream << "[ " << it->first << " , "
        << it->second << " , \"" << fRunType << "\"]";
    }
    value_stream << " ]";
    manager.AddMetadata("runs",value_stream.str());
    value_stream.str("");

    // Add list of events
    value_stream << "[ ";
    for (auto it = fEventList.begin(); it != fEventList.end(); ++it) {
      if (it != fEventList.begin()) value_stream << ",\n";
      value_stream << *it;
    }
    value_stream << " ]";
    manager.AddMetadata("events", value_stream.str());
    value_stream.str("");

  } // function DUNEMetadata::fillMetadata

  void DUNEMetadata::beginJob()
  {

  } // function DUNEMetadata::beginJob

  void DUNEMetadata::collectMetadata(art::Event const & e)
  {
    MetadataManager & manager = MetadataManager::getInstance();

    if (fFirstEvent == -1) fFirstEvent = e.event();
    fLastEvent = e.event();
    ++fNEvts;

    fEventList.push_back(e.event());

    if (!fParsedHWConfig) {

      // Now we get the provenance information included
      // in the art event. This is the only way to access
      // hardware configuration information, since this
      // module runs on the EventBuilder process.

      fhicl::ParameterSet ps;

      // Metadata fields
      std::string daq_config_name = "unknown";
      std::string data_stream = "unknown";
      bool got_hw_config = false;
      bool inconsistent_hw_config = false;
      bool is_fake_data = false;
      std::string component_list = "unknown";
      std::vector<std::string> components;
      std::vector<std::string> hw_config_name_in = { "gain", "shape",
         "baselineHigh", "leakHigh", "leak10X", "acCouple", "pls_mode" };
      std::vector<std::string> hw_config_name_out = { "fegain", "feshapingtime",
        "febaselinehigh", "feleakhigh", "feleak10x", "accouple", "calibpulsemode" };
      std::vector<std::string> hw_config(hw_config_name_in.size());
      for (std::string & s : hw_config) s = "unknown";

      if (e.getProcessParameterSet("DAQ", ps)) {

        // The configuration documents are organised
        // as a parameter set where each key is a single
        // component, and the value is a string containing
        // the full fhicl configuration for that component.

        fhicl::ParameterSet config_docs; 
        if (ps.get_if_present<fhicl::ParameterSet>("configuration_documents", config_docs)) {

          // Look for DAQ config name in metadata config dump
          std::string config_metadata_string;
          if (config_docs.get_if_present<std::string>("metadata", config_metadata_string)) {
            daq_config_name = GetOneInstance(config_metadata_string, "Config name");

          } // if found metadata.contents

          // Look for the run type in RunControl config dump
          std::string config_runcontrol_string;
          if (config_docs.get_if_present<std::string>("RunControl", config_runcontrol_string)) {
            data_stream = GetOneInstance(config_runcontrol_string, "run_type");

          } // if found RunControl string

          // Loop over every key
          for (std::string key : config_docs.get_all_keys()) {

            // Get the config dump for this component
            std::string config_dump = config_docs.get<std::string>(key, "");

            std::string id_name = GetOneInstance(config_dump, "IDName");
            if (id_name != "" && std::find(components.begin(), components.end(), id_name) == components.end())
              components.push_back(id_name);

            // If the key is a WIB, get config information
            if (key.find("wib") != std::string::npos) {

              // Keep track of whether HW config was set this cycle
              bool setting_hw_config = false;

              // Loop over each piece of HW config information
              for (unsigned int i = 0; i < hw_config.size(); ++i) {

                // Get all instances of this information
                std::vector<std::string> vals = GetAllInstances(config_dump, hw_config_name_in[i]);

                // If there aren't any, skip
                if (vals.size() == 0) continue;

                // If we already got HW information, just check it matches
                if (got_hw_config) {
                  for (std::string val : vals) {
                    if (val != hw_config[i]) {
                      inconsistent_hw_config = true;
                      break;
                    }
                  }
                }

                // If HW information is not set yet, set it
                else {
                  setting_hw_config = true;
                  hw_config[i] = vals[0];
                  for (unsigned int it = 1; it < vals.size(); ++it) {
                    if (vals[it] != hw_config[i]) {
                      inconsistent_hw_config = true;
                      break;
                    }
                  }
                }

                // If we found an inconsistency, stop the loop
                if (inconsistent_hw_config) break;

              } // for i in hw_config.size()

              // If HW config was set by this component, store that for future components
              if (setting_hw_config) got_hw_config = true;

            } // if pset key contains wib

            // Break the loop if HW config info is inconsistent
            if (inconsistent_hw_config) break;

            // Finally, check the fake data flags for this config dump
            std::vector<std::string> fake_data = GetAllInstances(config_dump, "enable_fake_data");
            for (std::string val : fake_data)
              if (val != "false")
                is_fake_data = true;

          } // for each key in config_docs

          // If we found components, add them all to a single string
          if (components.size() > 0) {
            component_list = "";
            for (std::string component : components)
              component_list = component_list + component + ":";
            component_list = component_list.substr(0, component_list.length() - 1);
          }
        }
      } // if get_if_present(config docs)

      // Add HW config as metadata
      manager.AddMetadata("data_stream", data_stream);
      manager.AddMetadata("dune_data.daqconfigname", daq_config_name);
      manager.AddMetadata("dune_data.detector_config", component_list);
      manager.AddMetadata("dune_data.inconsistent_hw_config", std::to_string(inconsistent_hw_config));
      manager.AddMetadata("dune_data.is_fake_data", std::to_string(is_fake_data));
      if (got_hw_config && !inconsistent_hw_config) {
        for (unsigned int i = 0; i < hw_config.size(); ++i) {
          manager.AddMetadata("dune_data." + hw_config_name_out[i], hw_config[i]);
        }
      }

      // Flag this as done, so it's only done once
      fParsedHWConfig = true;
    }

    std::cout << "Done with collectMetadata function." << std::endl;

  } // function DUNEMetadata::collectMetadata

  void DUNEMetadata::beginSubRun(art::SubRun const & sr)
  {
    fSubRuns.insert(std::make_pair(sr.run(),sr.subRun()));

  } // function DUNEMetadata::beginSubRun

  bool DUNEMetadata::GetExistingMetadata(std::string key, std::string & val)
  {
    // Get a handle to existing metadata
    art::FileCatalogMetadata::collection_type coll;
    art::ServiceHandle<art::FileCatalogMetadata const>{}->getMetadata(coll);

    // Search for the given key
    auto it = std::find_if(coll.begin(), coll.end(),
      [&key](const std::pair<std::string, std::string>& p){ return p.first == key;});

    // If we don't find it, quit 
    if (it == coll.end()) return false;

    // Otherwise, set the value and return.
    val = it->second;
    return true;

  } // function DUNEMetadata::GetExistingMetadata

  std::string DUNEMetadata::GetOneInstance(std::string config_dump, std::string key)
  {
    // Return value
    std::string val = "";

    // Look for key and return empty string if we don't find it
    size_t pos = config_dump.find(key, 0);
    if (pos == std::string::npos) return val;

    // Get the start of the value from the start of the key
    size_t val_start = pos + key.size() + 2;

    // Find the end of the line and length of value
    size_t val_length = config_dump.find("\n", val_start) - val_start;

    // Get the value, strip out quotes if necessary, and return it
    val = config_dump.substr(val_start, val_length);
    if (val.substr(0,1) == "\"")
      val = val.substr(1, val.length()-2);
    return val;

  } // function DUNEMetadata::GetOneInstance

  std::vector<std::string> DUNEMetadata::GetAllInstances(std::string config_dump, std::string key)
  {
    // Return vector
    std::vector<std::string> vals;

    // Find first instance
    size_t pos = config_dump.find(key, 0);

    // Loop over the whole string
    while (pos != std::string::npos) {

      // Get the start and length of the value
      size_t val_start = pos + key.size() + 2;
      size_t val_end = config_dump.find("\n", val_start);
      size_t val_length = val_end - val_start;

      // Pull out the value
      std::string val = config_dump.substr(val_start, val_length);
      if (val.substr(0,1) == "\"")
        val = val.substr(1, val.length()-2);
      vals.push_back(config_dump.substr(val_start, val_length));

      // Look for the next instance
      pos = config_dump.find(key, val_end);
    }

    // Give back the vector
    return vals;

  } // function DUNEMetadata::GetAllInstances

  DEFINE_ART_FILECATALOGMETADATA_PLUGIN(DUNEMetadata)

} // namespace meta

