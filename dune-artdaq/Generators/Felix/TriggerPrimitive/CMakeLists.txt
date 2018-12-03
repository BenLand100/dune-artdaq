# -*- mode: cmake-mode -*-

art_make_library( LIBRARY_NAME dune-artdaq_Generators_Felix_TriggerPrimitive_channelmap
		  SOURCE PdspChannelMapService.cpp
)

add_subdirectory(tests)
