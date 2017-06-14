# Main library.


# Subdirectories.
add_subdirectory(ArtModules)
add_subdirectory(Generators)
add_subdirectory(BuildInfo)
add_subdirectory(Triggers)
add_subdirectory(Plugins)
add_subdirectory(DAQLogger)

# JCF, Jun-14-2017

# Disabling the monitoring directories as they contain code written
# for 35ton. However, I'm not deleting them as some of the code may
# still be relevant for protoDUNE

#add_subdirectory(OnlineMonitoring)
#add_subdirectory(PedestalMonitoring)

if (NOT ZEROMQ_IGNORED)
add_subdirectory(RCConnection)
endif()
