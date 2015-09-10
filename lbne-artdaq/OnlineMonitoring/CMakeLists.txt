art_make_library(LIBRARY_NAME lbne-artdaq_OnlineMonitoring_DataReformatter
		 SOURCE DataReformatter.cxx
		 LIBRARIES lbne-raw-data_Overlays
			   lbne-artdaq_OnlineMonitoring_EventDisplay
			   lbne-artdaq_OnlineMonitoring_ChannelMap
			   ${CETLIB}
			   ${ROOT_BASIC_LIB_LIST}
)

art_make_library(LIBRARY_NAME lbne-artdaq_OnlineMonitoring_MonitoringData
		 SOURCE MonitoringData.cxx
		 LIBRARIES lbne-artdaq_OnlineMonitoring_DataReformatter
 		           ${ROOT_BASIC_LIB_LIST}
 			   ${MF_MESSAGELOGGER}
			   ${MF_UTILITIES}
)

art_make_library(LIBRARY_NAME lbne-artdaq_OnlineMonitoring_EventDisplay
		 SOURCE EventDisplay.cxx
		 LIBRARIES ${ROOT_BASIC_LIB_LIST}
)

art_make_library(LIBRARY_NAME lbne-artdaq_OnlineMonitoring_ChannelMap
		 SOURCE ChannelMap.cxx
		 LIBRARIES ${ROOT_BASIC_LIB_LIST}
)

art_make_library(LIBRARY_NAME lbne-artdaq_OnlineMonitoring_OnlineMonitoringNamespace
		 SOURCE OnlineMonitoringNamespace.cxx
		 LIBRARIES ${ROOT_BASIC_LIB_LIST}
)

simple_plugin(OnlineMonitoring "module"
  ${SIMULATIONBASE}
  ${ART_FRAMEWORK_CORE}
  ${ART_FRAMEWORK_PRINCIPAL}
  ${ART_FRAMEWORK_SERVICES_REGISTRY}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
  ${ART_FRAMEWORK_SERVICES_OPTIONAL_RANDOMNUMBERGENERATOR_SERVICE}
  ${ART_PERSISTENCY_COMMON}
  ${ART_PERSISTENCY_PROVENANCE}
  ${ART_UTILITIES}
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${ROOT_BASIC_LIB_LIST}
  #  lbne-artdaq_RCConnection
  #  lbne-artdaq_I3JSON
  lbne-raw-data_Overlays
  lbne-artdaq_OnlineMonitoring_MonitoringData
  lbne-artdaq_OnlineMonitoring_DataReformatter
  lbne-artdaq_OnlineMonitoring_EventDisplay
  lbne-artdaq_OnlineMonitoring_ChannelMap
)
