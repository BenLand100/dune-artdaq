
art_make(NO_PLUGINS # WAnt different link lists for modues
  LIB_LIBRARIES lbne-raw-data_Overlays lbne-artdaq_Products
)

simple_plugin(MilliSliceDump "module" lbne-raw-data_Overlays )

simple_plugin(TpcMilliSliceDump "module" lbne-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(PennMilliSliceDump "module" lbne-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(SSPDump "module" lbne-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(SparsifySSP "module" lbne-raw-data_Overlays 
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(ToyDump "module")

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


simple_plugin(LbneArtdaqBuildInfo "module"
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  lbne-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

