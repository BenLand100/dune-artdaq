simple_plugin(ExampleTrigger "module"
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  lbne-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

simple_plugin(PreScaleTrigger "module"
  artdaq_DAQdata
  artdaq_BuildInfo
  artdaq-core_BuildInfo
  lbne-artdaq_BuildInfo
  lbne-raw-data_BuildInfo
  ${MF_MESSAGELOGGER}
  ${FHICLCPP}
  ${CETLIB}
)

install_fhicl()

