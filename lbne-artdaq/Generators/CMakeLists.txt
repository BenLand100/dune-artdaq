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

# simple_plugin(Playback "generator"
#   lbne-artdaq_DAQLogger
#   lbne-raw-data_Overlays
#   ${ART_PERSISTENCY_PROVENANCE}
#   artdaq-core_Data_dict
#   art_Framework_IO_Root
#   art_Framework_Core
#   art_Framework_IO_Sources
#   art_Framework_Principal
#   art_Persistency_Provenance
#   art_Utilities
#   ${ARTDAQ_APPLICATION}
#   ${ARTDAQ_DAQDATA}
#   ${ARTDAQ-CORE_UTILITIES}
#   ${ART_UTILITIES}
#   ${FHICLCPP}
#   ${CETLIB}
#   ${ROOT_CINTEX}
#   ${ROOT_RIO}
#   ${ROOT_CORE}
#   ${ROOT_NET}
#   ${ROOT_REFLEX}
#   ${Boost_SYSTEM_LIBRARY}
#   )


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
