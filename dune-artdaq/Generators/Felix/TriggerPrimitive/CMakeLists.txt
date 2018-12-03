# -*- mode: cmake-mode -*-

find_package(Threads)

art_make_library( LIBRARY_NAME dune-artdaq_Generators_Felix_TriggerPrimitive_channelmap
		  SOURCE PdspChannelMapService.cpp
)

art_make_library( LIBRARY_NAME dune-artdaq_Generators_Felix_TriggerPrimitive
		  SOURCE frame_expand.cpp process_avx2.cpp design_fir.cpp TriggerPrimitiveFinder.cpp
                  LIBRARIES
                  ${CMAKE_THREAD_LIBS_INIT}
                  ${Boost_SYSTEM_LIBRARY}
                  ${Boost_THREAD_LIBRARY}
)

add_subdirectory(tests)
