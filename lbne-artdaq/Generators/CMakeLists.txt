simple_plugin(SSP "generator"
  lbne-artdaq_Overlays
  lbne-artdaq_Generators_anlBoard
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP} 
  ${CETLIB} 
  )

simple_plugin(ToySimulator "generator"
  lbne-artdaq_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${CETLIB}
  )


  add_subdirectory(anlBoard)
