art_make(
	LIB_LIBRARIES
	${ART_UTILITIES}
	${FHICLCPP}
	${CETLIB}
	${MF_MESSAGELOGGER}
	${MF_UTILITIES}
	${Boost_SYSTEM_LIBRARY}
	-lpthread
	)
	
 add_subdirectory(tools)