
art_make( LIBRARY_NAME  OnlineMonitoring
	  LIB_LIBRARIES lbne-raw-data_Overlays
			${CETLIB}
			${ROOT_BASIC_LIB_LIST}
 			${MF_MESSAGELOGGER}
			${MF_UTILITIES}
	  MODULE_LIBRARIES OnlineMonitoring
	  		   #lbne-raw-data_Services_ChannelMap_ChannelMapService_service
	                   ${ART_FRAMEWORK_CORE}
			   ${ART_FRAMEWORK_PRINCIPAL}
			   ${ART_FRAMEWORK_SERVICES_REGISTRY}
 			   ${ART_FRAMEWORK_SERVICES_OPTIONAL}
  			   ${ART_FRAMEWORK_SERVICES_OPTIONAL_TFILESERVICE_SERVICE}
  			   ${ART_FRAMEWORK_SERVICES_OPTIONAL_RANDOMNUMBERGENERATOR_SERVICE}
  			   ${ART_PERSISTENCY_COMMON}
 			   ${ART_PERSISTENCY_PROVENANCE}
			   ${ART_UTILITIES}
  			   #  lbne-artdaq_RCConnection
  			   #  lbne-artdaq_I3JSON
)
