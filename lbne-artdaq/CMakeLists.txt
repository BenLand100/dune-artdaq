# Main library.


# Subdirectories.
add_subdirectory(ArtModules)
add_subdirectory(Generators)
add_subdirectory(BuildInfo)
add_subdirectory(Triggers)
add_subdirectory(Plugins)
add_subdirectory(DAQLogger)
add_subdirectory(OnlineMonitoring)

if (NOT ZEROMQ_IGNORED)
add_subdirectory(RCConnection)
endif()
