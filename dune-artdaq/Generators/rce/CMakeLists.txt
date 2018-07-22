include_directories($ENV{ROGUE_INC})
cet_find_library(ROGUE_LIB NAMES rogue-core PATHS ENV ROGUE_LIB NO_DEFAULT_PATH)
message("Rogue Lib: ${ROGUE_LIB}")

add_definitions(-DNO_PYTHON)
art_make(
	LIB_LIBRARIES
   artdaq-core_Data
   artdaq_Application
   artdaq_DAQdata
   artdaq-core_Utilities
   ${CANVAS}
   ${FHICLCPP}
   ${CETLIB}

	${Boost_SYSTEM_LIBRARY}
   ${Boost_THREAD_LIBRARY}
   ${Boost_ATOMIC_LIBRARY}
   ${ROGUE_LIB}
   pthread
)
