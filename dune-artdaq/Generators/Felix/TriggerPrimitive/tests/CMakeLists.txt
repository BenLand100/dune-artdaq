# -*- mode: cmake-mode -*-

find_package(Threads)

set(TP_LIBS dune-artdaq_Generators_Felix_TriggerPrimitive_channelmap
  dune-artdaq_Generators_Felix_TriggerPrimitive
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_THREAD_LIBRARY}
  ${Boost_PROGRAM_OPTIONS_LIBRARY}
  ${netio_LIBRARY_DIR}/libnetio.so
  ${netio_LIBRARY_DIR}/libfabric.so
  ${CMAKE_THREAD_LIBS_INIT}
)

cet_make_exec(run_on_fragment 
  SOURCE run_on_fragment.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(read_frames 
  SOURCE read_frames.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(run_trigprim
  SOURCE run_trigprim.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(collection_reorder 
  SOURCE collection_reorder.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(expand_collection_speed
  SOURCE expand_collection_speed.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(find_collection
  SOURCE find_collection.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(number_collection
  SOURCE number_collection.cpp
  LIBRARIES ${TP_LIBS}
)
