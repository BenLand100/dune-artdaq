


art_make( BASENAME_ONLY
  LIBRARY_NAME dune-artdaq_ArtModules
  NO_PLUGINS # Want different link lists for modules
  DICT_DICTIONARIES
  ${ART_PERSISTENCY_PROVENANCE}
  ${CANVAS}
  LIB_LIBRARIES dune-raw-data_Overlays
  artdaq_ArtModules
  art_Framework_Core
  art_Framework_IO_Sources
  art_Framework_Principal
  art_Persistency_Provenance
  MF_MessageLogger
  ${ROOT_CINTEX}
  ${ROOT_RIO}
  ${ROOT_CORE}
  ${ROOT_NET}
  ${ROOT_REFLEX}
  cetlib
)

simple_plugin(MilliSliceDump "module" dune-raw-data_Overlays )

simple_plugin(TpcMilliSliceDump "module" dune-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(RCEDiagnostic "module" dune-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(PennMilliSliceDump "module" dune-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
  dune-artdaq_DAQLogger
)

simple_plugin(SSPDump "module" dune-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(SparsifySSP "module" dune-raw-data_Overlays 
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(ToyDump "module")

simple_plugin(DuneInput "source"
  dune-raw-data_Overlays
  artdaq_ArtModules
  artdaq-core_Core
  artdaq-core_Utilities
  artdaq_DAQdata
  artdaq-utilities_Plugins
  art_Framework_Core
  art_Framework_IO_Sources
  art_Framework_Principal
  art_Persistency_Provenance
  art_Utilities
  MF_MessageLogger
)

# simple_plugin(BasicMetrics "module"
#   ${ART_FRAMEWORK_SERVICES_REGISTRY}
#   ${ART_FRAMEWORK_SERVICES_OPTIONAL}
#   artdaq_DAQrate
#   ${MF_MESSAGELOGGER}
#   ${MF_UTILITIES}
#   ${FHICLCPP}
# )

# Subdirectories.
add_subdirectory(DQM)


simple_plugin(DuneArtdaqBuildInfo "module"
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  dune-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

