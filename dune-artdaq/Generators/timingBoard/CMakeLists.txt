art_make_library( SOURCE InhibitGet.c StatusPublisher.cc
		  LIBRARIES ${UHAL_UHAL} ${UHAL_LOG} ${Boost_SYSTEM_LIBRARY} zmq dune-artdaq_DAQLogger
		  LIBRARY_NAME_VAR TIMING_BOARD_LIB )


cet_make_exec(TimingMain
  SOURCE TimingMain.cc
  LIBRARIES   dune-raw-data_Overlays
  dune-artdaq_Generators_timingBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  dune-artdaq_DAQLogger
  ${UHAL_UHAL}
  ${UHAL_LOG}
  ${PDT_CORE}
  dune-artdaq_Generators_TimingReceiver_generator)
