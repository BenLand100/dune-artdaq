simple_plugin(TPCMilliSliceSimulatorWithCopy "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Data
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  ${BOOST_SYSTEM}
  ${CETLIB_EXCEPT}
  )

simple_plugin(SSP "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_anlBoard
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP} 
  ${CETLIB} 
  dune-artdaq_DAQLogger
  ${CETLIB_EXCEPT}
  )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-pointer-arith")
include_directories($ENV{FELIX_DEPS_INC})
add_subdirectory(Felix)

simple_plugin(FelixReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_Felix
  dune-artdaq_DAQLogger
  artdaq_Application
  artdaq_Generators
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
  


simple_plugin(HitFinderCPUReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_DAQLogger
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${PTMP_LIBRARIES}
  dune-artdaq_DAQLogger
)

add_subdirectory(ToyHardwareInterface)
simple_plugin(ToySimulator "generator"
  dune-artdaq_Generators_ToyHardwareInterface
  dune-raw-data_Overlays
  dune-artdaq_DAQLogger
  InhibitMaster_CommonTools
  artdaq_Application
  artdaq_Generators
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
  ${BOOST_SYSTEM}
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
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Utilities
  dune-artdaq_DAQLogger
  artdaq-utilities_Plugins
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
  artdaq_Generators
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
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${MF_MESSAGELOGGER}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${JSONCPP}
  )

add_subdirectory(timingBoard)

include_directories($ENV{SMC_HOME}/lib/C++)
simple_plugin(TimingReceiver "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_timingBoard
  artdaq-core_Data
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq_DAQrate
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

add_subdirectory(swTrigger)

simple_plugin(Candidate "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${PTMP_LIBRARIES}
  dune-artdaq_DAQLogger   
)

simple_plugin(IsoMuonFinder "generator"
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  dune-artdaq_Generators_timingBoard
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${PTMP_LIBRARIES}
  dune-artdaq_DAQLogger   
)

simple_plugin(SWTrigger "generator"
  dune-raw-data_Overlays
  dune-artdaq_Generators_timingBoard
  artdaq_Application
  artdaq_Generators
  artdaq_DAQdata
  artdaq_DAQrate
  artdaq-core_Utilities
  artdaq-core_Data
  artdaq-utilities_Plugins
  ${CANVAS}
  ${FHICLCPP}
  ${CETLIB}
  ${CETLIB_EXCEPT}
  ${PTMP_LIBRARIES}
  dune-artdaq_DAQLogger
  fhicl-to-json
)

if ( (DEFINED ENV{PROTODUNE_WIBSOFT_LIB}) AND (DEFINED ENV{PROTODUNE_WIBSOFT_INC}) )

include_directories( $ENV{PROTODUNE_WIBSOFT_INC} )

simple_plugin(WIBReader "generator"
         $ENV{PROTODUNE_WIBSOFT_LIB}/libWIB.so
         $ENV{PROTODUNE_WIBSOFT_LIB}/libToolException.so
         artdaq_Application
         artdaq_Generators
	 dune-artdaq_DAQLogger
  	 ${CETLIB_EXCEPT}
  	 ${CETLIB}
  	 ${FHICLCPP}
         ${Boost_SYSTEM_LIBRARY}
)

else()

message( FATAL_ERROR "You need environment variables called PROTODUNE_WIBSOFT_LIB and PROTODUNE_WIBSOFT_INC that are set by the protodune_wibsoft product" )

endif()


add_subdirectory(CRTInterface)
simple_plugin(CRTFragGen "generator"
  dune-artdaq_Generators_CRTInterface
  dune-raw-data_Overlays
  artdaq_Application
  artdaq_Generators
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
 

