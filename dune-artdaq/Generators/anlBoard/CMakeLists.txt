art_make(LIBRARY_NAME dune-artdaq_Generators_anlBoard
        LIB_LIBRARIES
	${CMAKE_CURRENT_SOURCE_DIR}/lib/libftd2xx.so
	${CANVAS}
        ${FHICLCPP}
        ${CETLIB}
        ${Boost_SYSTEM_LIBRARY}
 	dune-artdaq_DAQLogger
        -lpthread
        )

install_headers()
