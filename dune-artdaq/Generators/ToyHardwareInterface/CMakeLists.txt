art_make( LIBRARY_NAME dune-artdaq_Generators_ToyHardwareInterface
        LIB_LIBRARIES
        ${ART_UTILITIES}
        ${FHICLCPP}
        ${CETLIB}
        ${CETLIB_EXCEPT}
	artdaq-core_Data
	dune-raw-data_Overlays
        )

install_headers()
