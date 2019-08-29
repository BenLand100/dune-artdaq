# -*- mode: cmake-mode -*-

find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mno-avx512f -mno-avx512er -mno-avx512cd -mno-avx512pf -mno-avx512dq -mno-avx512bw -mno-avx512vl -mno-avx512ifma -mno-avx512vbmi -mtune=native")

art_make_library( LIBRARY_NAME dune-artdaq_Generators_Felix_TriggerPrimitive_channelmap
		  SOURCE PdspChannelMapService.cpp
)

art_make_library( LIBRARY_NAME dune-artdaq_Generators_Felix_TriggerPrimitive
		  SOURCE frame_expand.cpp process_avx2.cpp design_fir.cpp TriggerPrimitiveFinder.cpp
                  LIBRARIES
                  artdaq_DAQdata             # For metricMan
                  artdaq-utilities_Plugins   # For metricMan
                  dune-artdaq_Generators_Felix_TriggerPrimitive_channelmap
                  dune-artdaq_DAQLogger
                  ${CMAKE_THREAD_LIBS_INIT}
                  ${Boost_SYSTEM_LIBRARY}
                  ${CETLIB}
                  ${CETLIB_EXCEPT}
                  ${FHICLCPP}
                  artdaq-core_Data
                  ${PTMP_LIBRARIES}
                  numa
)

add_subdirectory(tests)
