
MESSAGE(INFO " Netio_LIBRARY_DIR " ${netio_LIBRARY_DIR}) 
MESSAGE(INFO " Netio_INCLUDE_DIR " ${netio_INCLUDE_DIR}) 

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
        pthread
        tbb
        zmq
)

