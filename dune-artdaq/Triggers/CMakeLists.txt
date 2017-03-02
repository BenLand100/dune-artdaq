art_make(NO_PLUGINS # Want different link lists for modules
  LIB_LIBRARIES lbne-raw-data_Overlays dune-artdaq_Products
)

simple_plugin(ExampleTrigger "module" lbne-raw-data_Overlays 
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(SSPTrigger "module" lbne-raw-data_Overlays 
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(PennBoardTrigger "module" 
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  lbne-raw-data_Overlays 
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(PreScaleTrigger "module"
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(TriggerSelector "module" 
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  dune-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  lbne-raw-data_Overlays 
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

install_fhicl()

