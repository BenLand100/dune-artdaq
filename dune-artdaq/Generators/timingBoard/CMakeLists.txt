art_make_library( SOURCE I2CuHal.cpp si5344.cpp TimingSequence.cpp
		  LIBRARIES ${UHAL_UHAL} ${UHAL_LOG} ${Boost_SYSTEM_LIBRARY}
		  LIBRARY_NAME_VAR TIMING_BOARD_LIB )


