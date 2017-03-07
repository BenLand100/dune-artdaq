simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Data
  artdaq-core_Utilities
  canvas_Utilities
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  )

simple_plugin(SSP "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_anlBoard
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  canvas_Utilities
  ${FHICLCPP} 
  ${CETLIB} 
  dune-artdaq_DAQLogger
  )

simple_plugin(ToySimulator "generator"
  dune-artdaq_Generators_ToyHardwareInterface
  dune-raw-data_Overlays
  dune-artdaq_DAQLogger
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  canvas_Utilities
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  )


simple_plugin(Playback "generator"
  dune-artdaq_DAQLogger
  dune-raw-data_Overlays
  artdaq-core_Data
  canvas_Utilities
  canvas_Persistency_Common
  canvas_Persistency_Provenance
  art_Framework_IO_Sources
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  canvas_Utilities
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
  dune-raw-data_Overlays
  dune-artdaq_Generators_RceSupportLib
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  canvas_Utilities
  ${FHICLCPP}
  ${CETLIB}
  dune-artdaq_DAQLogger
  )
  
  add_subdirectory(RceSupportLib)

simple_plugin(PennReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_pennBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  canvas_Utilities
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  dune-artdaq_DAQLogger
  )


  add_subdirectory(pennBoard)
  add_subdirectory(ToyHardwareInterface)
