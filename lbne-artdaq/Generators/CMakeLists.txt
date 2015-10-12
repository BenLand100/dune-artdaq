simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  )

simple_plugin(SSP "generator"
  lbne-raw-data_Overlays
  lbne-artdaq_Generators_anlBoard
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP} 
  ${CETLIB} 
  )

simple_plugin(ToySimulator "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${CETLIB}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  lbne-artdaq_DAQLogger
  )

  add_subdirectory(anlBoard)
  
simple_plugin(TpcRceReceiver "generator"
  lbne-raw-data_Overlays
  lbne-artdaq_Generators_RceSupportLib
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  lbne-artdaq_DAQLogger
  )
  
  add_subdirectory(RceSupportLib)

simple_plugin(PennReceiver "generator"
  lbne-raw-data_Overlays
  lbne-artdaq_Generators_pennBoard
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  )


  add_subdirectory(pennBoard)
