
# JCF, 7/6/15

# Only build the DAQLogger with run control communication capability
# (i.e., with the dune-artdaq_RCConnection library) if ZEROMQ_IGNORED
# has not been set in the base CMakeLists.txt file

if (NOT ZEROMQ_IGNORED) 

art_make_library( LIBRARY_NAME dune-artdaq_DAQLogger
		  SOURCE
		  DAQLogger.cc
                  LIBRARIES
		  ${MF_MESSAGELOGGER}
		  ${MF_MESSAGEUTILITIES}
		  dune-artdaq_RCConnection
		  )

else()

art_make_library( LIBRARY_NAME dune-artdaq_DAQLogger
		  SOURCE
		  DAQLogger.cc
                  LIBRARIES
		  ${MF_MESSAGELOGGER}
		  ${MF_MESSAGEUTILITIES}
		  )

endif()
