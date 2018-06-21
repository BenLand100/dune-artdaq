art_make( LIBRARY_NAME dune-artdaq_Generators_CRTInterface
  LIB_LIBRARIES
  art_Utilities
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${MF_MESSAGELOGGER}
  artdaq-core_Data
  dune-raw-data_Overlays
  ${Boost_SYSTEM_LIBRARY}
  )
