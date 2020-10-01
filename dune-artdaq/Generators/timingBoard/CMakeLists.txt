art_make_library( LIBRARY_NAME dune-artdaq_Generators_timingBoard
		  SOURCE InhibitGet.c StatusPublisher.cc FragmentPublisher.cc HwClockPublisher.cc HwClockSubscriber.cc
		  LIBRARIES ${UHAL_UHAL}
      ${UHAL_LOG}
      ${Boost_SYSTEM_LIBRARY}
      ${LIBZMQ}
      dune-artdaq_DAQLogger
      ${CETLIB}
      ${CETLIB_EXCEPT}
)
		  #LIBRARY_NAME_VAR TIMING_BOARD_LIB )

# add_subdirectory(test)


# cet_make_exec(TimingMain
#   SOURCE TimingMain.cc
#   LIBRARIES   dune-raw-data_Overlays
#   dune-artdaq_Generators_timingBoard
#   artdaq-core_Data
#   artdaq_Application
#   artdaq_DAQdata
#   artdaq-core_Utilities
#   ${CANVAS}
#   ${FHICLCPP}
#   ${MF_MESSAGELOGGER}
#   ${MF_UTILITIES}
#   ${CETLIB}
#   dune-artdaq_DAQLogger
#   ${UHAL_UHAL}
#   ${UHAL_LOG}
#   ${PDT_CORE}
#   ${Boost_SYSTEM_LIBRARY}
#   dune-artdaq_Generators_TimingReceiver_generator
#   pthread
# )

cet_make_exec(FragmentSubscriber
  SOURCE FragmentSubscriber.cc
  LIBRARIES   dune-raw-data_Overlays
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${Boost_SYSTEM_LIBRARY}
  ${LIBZMQ})
