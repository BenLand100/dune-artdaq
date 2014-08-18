
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

simple_plugin(SSPDump "module" lbne-raw-data_Overlays 
  ${ROOT_HIST}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
)

simple_plugin(ToyDump "module")

# Subdirectories.
add_subdirectory(DQM)


simple_plugin(LbneArtdaqBuildInfo "module"
  artdaq_DAQdata
  lbne-artdaq_Version
  lbne-raw-data_Version
  artdaq-core_Data
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(LbneArtdaqPrintVersionInfo "module"
  artdaq_DAQdata
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

