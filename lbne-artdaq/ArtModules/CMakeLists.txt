art_make(NO_PLUGINS # WAnt different link lists for modues
  LIB_LIBRARIES lbne-artdaq_Overlays lbne-artdaq_Products
)

simple_plugin(MilliSliceDump "module" lbne-artdaq_Overlays )
simple_plugin(ToyDump "module")


# Subdirectories.
add_subdirectory(DQM)
