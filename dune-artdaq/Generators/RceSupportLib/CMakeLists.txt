#execute_process(COMMAND xml2-config --libs OUTPUT_VARIABLE LIBXML2_LIBS)
#MESSAGE( STATUS "LIBXML2_LIBS: " ${LIBXML2_LIBS} "<<<")

art_make(LIBRARY_NAME dune-artdaq_Generators_RceSupportLib
	LIB_LIBRARIES
	${CANVAS}
	${FHICLCPP}
	${CETLIB}
	$ENV{LIBXML2_FQ_DIR}/lib/libxml2.so
	${MF_MESSAGELOGGER}
	${MF_UTILITIES}
	${Boost_SYSTEM_LIBRARY}
	dune-artdaq_DAQLogger
	-lpthread
	)
	
install_headers()

 add_subdirectory(tools)
