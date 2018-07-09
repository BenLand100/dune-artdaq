

simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Data
  artdaq-core_Utilities
  ${CANVAS}
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
  ${CANVAS}
  ${FHICLCPP} 
  ${CETLIB} 
  dune-artdaq_DAQLogger
  )

# simple_plugin(FelixReceiver "generator"
#   dune-raw-data_Overlays
#   dune-artdaq_Generators_Felix
#   dune-artdaq_DAQLogger
#   artdaq_Application
#   artdaq_DAQdata
#   artdaq-core_Utilities
#   artdaq-core_Data
#   ${CANVAS}
#   ${FHICLCPP}
#   ${CETLIB}
#   dune-artdaq_DAQLogger
# )
# add_subdirectory(Felix)

simple_plugin(ToySimulator "generator"
  dune-artdaq_Generators_ToyHardwareInterface
  dune-raw-data_Overlays
  dune-artdaq_DAQLogger
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  )

  add_subdirectory(anlBoard)
  
simple_plugin(TpcRceReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_RceSupportLib
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  ${CANVAS}
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
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  dune-artdaq_DAQLogger
  )

simple_plugin(CRTFragGen "generator"
  dune-artdaq_Generators_CRTInterface
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS_LIB_LIST}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  )

add_subdirectory(CRTInterface)

simple_plugin(TriggerBoardReader "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_TriggerBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  dune-artdaq_DAQLogger
  )


  add_subdirectory(pennBoard)
  add_subdirectory(TriggerBoard)
  add_subdirectory(ToyHardwareInterface)


add_subdirectory(timingBoard)

simple_plugin(TimingReceiver "generator"
  dune-raw-data_Overlays
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
  )

# The timing board reader #includes headers from uhal which generate
# compiler warnings, so we need to turn off -Werror for just this target

target_compile_options("dune-artdaq_Generators_TimingReceiver_generator" PRIVATE -Wno-error )

if ( DEFINED ENV{WIB_DIRECTORY} )

include_directories( $ENV{WIB_DIRECTORY}/WIB/include )
include_directories( $ENV{WIB_DIRECTORY}/exception/include )

simple_plugin(WIBReader "generator"
         $ENV{WIB_DIRECTORY}/WIB/lib/libWIB.so
         $ENV{WIB_DIRECTORY}/exception/lib/libToolException.so
         artdaq_Application
	 dune-artdaq_DAQLogger
)

else()

message( FATAL_ERROR "You need an environment variable called WIB_DIRECTORY whose value is the directory of the WIB code installation" )

endif()
