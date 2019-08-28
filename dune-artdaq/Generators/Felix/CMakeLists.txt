
MESSAGE(INFO " Netio_LIBRARY_DIR " ${netio_LIBRARY_DIR}) 
MESSAGE(INFO " Netio_INCLUDE_DIR " ${netio_INCLUDE_DIR}) 

MESSAGE(INFO "QAT DIR FOR INCLUDE: " $ENV{ICP_ROOT})
MESSAGE(INFO "QAT DIR FOR INCLUDE: " $ENV{ICP_ROOT}/qatzip2/qatzip/include )
include_directories( SYSTEM $ENV{ICP_ROOT}/qatzip2/qatzip/include )

#MESSAGE(INFO "FELIX-DEPS INC: " $ENV{FELIX_DEPS_INC})
#MESSAGE(INFO "FELIX-DEPS LIB: " $ENV{FELIX_DEPS_LIB})
#include_directories( SYSTEM $ENV{FELIX_DEPS_INC})

MESSAGE(INFO " PROTODUNE_FELIX_DEPS_LIB: " $ENV{PROTODUNE_FELIX_DEPS_LIB})
MESSAGE(INFO " PROTODUNE_FELIX_DEPS_INC: " ${PROTODUNE_FELIX_DEPS_INC})
include_directories( ${PROTODUNE_FELIX_DEPS_INC} )


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")

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
        ${netio_LIBRARY_DIR}/libnetio.so
        ${netio_LIBRARY_DIR}/libfabric.so
        $ENV{ICP_ROOT}/qatzip2/qatzip/src/libqatzip.so
        $ENV{ICP_ROOT}/build/libqat_s.so
        $ENV{ICP_ROOT}/build/libusdm_drv_s.so

        # FELIX deps
        $ENV{PROTODUNE_FELIX_DEPS_LIB}/libpacketformat.so
        $ENV{PROTODUNE_FELIX_DEPS_LIB}/libFlxCard.so
        $ENV{PROTODUNE_FELIX_DEPS_LIB}/libdrivers_rcc.so
        $ENV{PROTODUNE_FELIX_DEPS_LIB}/libcmem_rcc.so
        $ENV{PROTODUNE_FELIX_DEPS_LIB}/librcc_error.so
        # FELIX deps

        pthread
        tbb
        zmq
    )
endif()

