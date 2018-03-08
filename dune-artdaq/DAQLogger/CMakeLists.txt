
art_make_library( LIBRARY_NAME dune-artdaq_DAQLogger
		  SOURCE
		  DAQLogger.cc
                  LIBRARIES
		  ${MF_MESSAGELOGGER}
		  ${MF_MESSAGEUTILITIES}
		  )
