
art_make_library( LIBRARY_NAME lbne-artdaq_DAQLogger
		  SOURCE
		  DAQLogger.cc
                  LIBRARIES
		  ${MF_MESSAGELOGGER}
		  ${MF_MESSAGEUTILITIES}
		  lbne-artdaq_RCConnection
		  )
