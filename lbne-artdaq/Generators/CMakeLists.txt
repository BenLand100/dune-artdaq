
# simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
#   lbne-artdaq_Overlays
#   ${ARTDAQ_APPLICATION}
#   ${ARTDAQ_DAQDATA}
#   ${ARTDAQ_UTILITIES}
#   ${ART_UTILITIES}
#   ${FHICLCPP}
#   ${MF_MESSAGELOGGER}
#   ${CETLIB}
#   )



simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  lbne-raw-data_Overlays
  ${ARTDAQ_APPLICATION}
  ${ARTDAQ_DAQDATA}
  ${ARTDAQ_UTILITIES}
  ${ART_UTILITIES}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
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
