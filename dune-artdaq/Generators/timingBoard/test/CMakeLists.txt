cet_make_exec(HwClockTester
  SOURCE HwClockTester.cc
  LIBRARIES   dune-raw-data_Overlays
  dune-artdaq_Generators_timingBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${Boost_SYSTEM_LIBRARY}
  pthread
)
