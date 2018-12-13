
MESSAGE(INFO " Netio_LIBRARY_DIR " ${netio_LIBRARY_DIR}) 
MESSAGE(INFO " Netio_INCLUDE_DIR " ${netio_INCLUDE_DIR}) 

MESSAGE(INFO " NETIO_INC_DIR " $ENV{NETIO_INC_DIR})
MESSAGE(INFO " NETIO_LIB_DIR " $ENV{NETIO_LIB_DIR})
MESSAGE(INFO " FABRIC_ROOT_DIR " $ENV{FABRIC_ROOT_DIR}) 

MESSAGE(INFO "QAT DIR FOR INCLUDE: " $ENV{ICP_ROOT})
MESSAGE(INFO "QAT DIR FOR INCLUDE: " $ENV{ICP_ROOT}/qatzip2/qatzip/include )
include_directories( SYSTEM $ENV{ICP_ROOT}/qatzip2/qatzip/include )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -DBOOST_THREAD_VERSION=4 -DBOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY=1 -DBOOST_THREAD_PROVIDES_EXECUTORS=1")

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
	dune-raw-data_Overlays
        dune-artdaq_Generators_Felix_TriggerPrimitive
        ${netio_LIBRARY_DIR}/libnetio.so
        ${netio_LIBRARY_DIR}/libfabric.so
        $ENV{ICP_ROOT}/qatzip2/qatzip/src/libqatzip.so
        $ENV{ICP_ROOT}/build/libqat_s.so
        $ENV{ICP_ROOT}/build/libusdm_drv_s.so
        pthread
        tbb
        zmq
    )
endif()

add_subdirectory(TriggerPrimitive)
