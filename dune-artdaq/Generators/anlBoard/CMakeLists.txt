
MESSAGE(INFO " ftd2xx_LIBRARY_DIR " ${ftd2xx_LIBRARY_DIR})
MESSAGE(INFO " ftd2xx_INCLUDE_DIR " ${ftd2xx_INCLUDE_DIR})

art_make(LIBRARY_NAME dune-artdaq_Generators_anlBoard
        LIB_LIBRARIES
	${ftd2xx_LIBRARY_DIR}/libftd2xx.so
	${CANVAS}
        ${FHICLCPP}
        ${CETLIB}
        ${Boost_SYSTEM_LIBRARY}
 	dune-artdaq_Generators_Felix
	dune-artdaq_DAQLogger
        -lpthread
        )

install_headers()
