
art_make(NO_PLUGINS # WAnt different link lists for modues
  LIB_LIBRARIES lbne-raw-data_Overlays lbne-artdaq_Products
)

simple_plugin(MilliSliceDump "module" lbne-raw-data_Overlays )
simple_plugin(TpcMilliSliceDump "module" lbne-raw-data_Overlays )

simple_plugin(ToyDump "module")

# Subdirectories.
add_subdirectory(DQM)
