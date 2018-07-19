
MESSAGE(INFO " Netio_LIBRARY_DIR " ${netio_LIBRARY_DIR}) 
MESSAGE(INFO " Netio_INCLUDE_DIR " ${netio_INCLUDE_DIR}) 

MESSAGE(INFO " NETIO_INC_DIR " $ENV{NETIO_INC_DIR})
MESSAGE(INFO " NETIO_LIB_DIR " $ENV{NETIO_LIB_DIR})
MESSAGE(INFO " FABRIC_ROOT_DIR " $ENV{FABRIC_ROOT_DIR}) 

if(DEFINED ENV{FABRIC_ROOT_DIR})
    message(STATUS "FABRIC_ROOT_DIR environment variable defined. Bypassing UPS netio...")

    include_directories( $ENV{NETIO_INC_DIR}/netio )
    include_directories( $ENV{FABRIC_ROOT_DIR}/include )
    art_make(
        LIB_LIBRARIES
        ${ART_UTILITIES}
        ${FHICLCPP}
        ${CETLIB}
        ${TRACE}
        artdaq-core_Data
        dune-artdaq_DAQLogger
        #artdaq-core-demo_Overlays
	dune-raw-data_Overlays

        #netio
        #${netio_LIBRARY_DIR}/libnetio.so

        # OVERWRITE FOR NETIO ON INFINIBAND
        $ENV{NETIO_LIB_DIR}/libnetio.so
        $ENV{FABRIC_ROOT_DIR}/lib/libfabric.so

        pthread
        tbb
        zmq
    )
else()
    art_make(
        LIB_LIBRARIES
        ${ART_UTILITIES}
        ${FHICLCPP}
        ${CETLIB}
        ${TRACE}
        artdaq-core_Data
        dune-artdaq_DAQLogger
        #artdaq-core-demo_Overlays
	dune-raw-data_Overlays
        #netio
        ${netio_LIBRARY_DIR}/libnetio.so
        ${netio_LIBRARY_DIR}/libfabric.so
        pthread
        tbb
        zmq
    )
endif()


