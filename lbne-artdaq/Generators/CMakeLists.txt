
simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  )

simple_plugin(ToySimulator "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${CETLIB}
  )

  
simple_plugin(TpcRceReceiver "generator"
  lbne-raw-data_Overlays
  lbne-artdaq_Generators_RceSupportLib
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  )
  
  add_subdirectory(RceSupportLib)
