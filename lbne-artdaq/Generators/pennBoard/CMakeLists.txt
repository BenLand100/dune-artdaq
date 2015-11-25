#execute_process(COMMAND xml2-config --libs OUTPUT_VARIABLE LIBXML2_LIBS)
#MESSAGE( STATUS "LIBXML2_LIBS: " ${LIBXML2_LIBS} "<<<")

art_make(
	LIB_LIBRARIES
	${ART_UTILITIES}
	${FHICLCPP}
	${CETLIB}
	$ENV{LIBXML2_FQ_DIR}/lib/libxml2.so
	${MF_MESSAGELOGGER}
	${MF_UTILITIES}
	${Boost_SYSTEM_LIBRARY}
	-lpthread
	lbne-raw-data_Overlays
	lbne-artdaq_DAQLogger
	)
	
 add_subdirectory(emulator)
