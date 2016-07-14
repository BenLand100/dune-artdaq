simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${CANVAS_UTILITIES}
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
  ${CANVAS_UTILITIES}
  ${FHICLCPP} 
  ${CETLIB} 
  lbne-artdaq_DAQLogger
  )

simple_plugin(ToySimulator "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${CANVAS_UTILITIES}
  ${FHICLCPP}
  ${CETLIB}
  lbne-artdaq_DAQLogger
  )

simple_plugin(Playback "generator"
  lbne-artdaq_DAQLogger
  lbne-raw-data_Overlays
  artdaq-core_Data
  canvas_Utilities
  canvas_Persistency_Common
  canvas_Persistency_Provenance
  art_Framework_IO_Sources
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${CANVAS_UTILITIES}
  ${GALLERY}
  ${FHICLCPP}
  ${CETLIB}
  ${ROOT_CINTEX}
  ${ROOT_RIO}
  ${ROOT_CORE}
  ${ROOT_NET}
  ${ROOT_REFLEX}
  )


  add_subdirectory(anlBoard)
  
simple_plugin(TpcRceReceiver "generator"
  lbne-raw-data_Overlays
  lbne-artdaq_Generators_RceSupportLib
  ${ARTDAQ-CORE_DATA}
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ-CORE_UTILITIES}
  ${CANVAS_UTILITIES}
  ${FHICLCPP}
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
  ${CANVAS_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  lbne-artdaq_DAQLogger
  )


  add_subdirectory(pennBoard)
