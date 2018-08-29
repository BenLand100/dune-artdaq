
simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Data
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  boost_system
  ${CETLIB_EXCEPT}
  )

simple_plugin(SSP "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_anlBoard
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP} 
  ${CETLIB} 
  dune-artdaq_DAQLogger
  ${CETLIB_EXCEPT}
  )

add_subdirectory(Felix)
simple_plugin(FelixReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_Felix
  dune-artdaq_DAQLogger
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  dune-artdaq_DAQLogger
)

  add_subdirectory(ToyHardwareInterface)
simple_plugin(ToySimulator "generator"
  dune-artdaq_Generators_ToyHardwareInterface
  dune-raw-data_Overlays
  dune-artdaq_DAQLogger
  InhibitMaster_CommonTools
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  boost_system
  )

  add_subdirectory(anlBoard)
  
include_directories($ENV{ROGUE_INC})
add_definitions(-DNO_PYTHON)
add_subdirectory(rce)
simple_plugin(TpcRceReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_rce
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  dune-artdaq_DAQLogger
  #artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  #${CETLIB_EXCEPT}
  ${Boost_DATE_TIME_LIBRARY}
  )
  

  add_subdirectory(pennBoard)
  add_subdirectory(TriggerBoard)

simple_plugin(PennReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_pennBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  dune-artdaq_DAQLogger
  )


simple_plugin(TriggerBoardReader "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_TriggerBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  jsoncpp
  )

add_subdirectory(timingBoard)
include_directories($ENV{SMC_HOME}/lib/C++)
simple_plugin(TimingReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_timingBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  dune-artdaq_DAQLogger
  ${UHAL_UHAL}
  ${UHAL_LOG}
  ${PDT_CORE}
  )

# The timing board reader #includes headers from uhal which generate
# compiler warnings, so we need to turn off -Werror for just this target

target_compile_options("dune-artdaq_Generators_TimingReceiver_generator" PRIVATE -Wno-error )

if ( (DEFINED ENV{PROTODUNE_WIBSOFT_LIB}) AND (DEFINED ENV{PROTODUNE_WIBSOFT_INC}) )

include_directories( $ENV{PROTODUNE_WIBSOFT_INC} )

simple_plugin(WIBReader "generator"
         $ENV{PROTODUNE_WIBSOFT_LIB}/libWIB.so
         $ENV{PROTODUNE_WIBSOFT_LIB}/libToolException.so
         artdaq_Application
	 dune-artdaq_DAQLogger
  	 ${CETLIB_EXCEPT}
  	 ${CETLIB}
  	 ${FHICLCPP}
	 boost_system
)

else()

message( FATAL_ERROR "You need environment variables called PROTODUNE_WIBSOFT_LIB and PROTODUNE_WIBSOFT_INC that are set by the protodune_wibsoft product" )

endif()


add_subdirectory(CRTInterface)
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
  ${UHAL_UHAL}
  ${UHAL_LOG}
  )
 

