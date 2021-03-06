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
  dune-artdaq_DAQLogger
  ${MF_MESSAGELOGGER}
  ${MF_UTILITIES}
  ${CETLIB}
  ${FHICLCPP}
)

cet_make_exec(numa-where
  SOURCE numa-where.cpp
  LIBRARIES ${TP_LIBS} numa
)

cet_make_exec(synced_replayer
  SOURCE synced_replayer.cpp
  LIBRARIES ${TP_LIBS} ${PTMP_LIBRARIES} ${LIBCZMQ}
)

cet_make_exec(dumped_hits_to_text
  SOURCE dumped_hits_to_text.cpp
  LIBRARIES ${TP_LIBS} ${PTMP_LIBRARIES} ${LIBCZMQ}
)

cet_make_exec(dumped_tpsets_to_text
  SOURCE dumped_tpsets_to_text.cpp
  LIBRARIES ${TP_LIBS} ${PTMP_LIBRARIES} ${LIBCZMQ}
)

cet_make_exec(run_on_fragment 
  SOURCE run_on_fragment.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(read_frames 
  SOURCE read_frames.cpp
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

cet_make_exec(number_all_channels
  SOURCE number_all_channels.cpp
  LIBRARIES ${TP_LIBS}
)

# cet_make_exec(make_unique_tpf
#   SOURCE make_unique_tpf.cpp
#   LIBRARIES ${TP_LIBS}
# )

# cet_make_exec(main_loop
#   SOURCE main_loop.cc
#   LIBRARIES ${TP_LIBS}
#   dune-artdaq_Generators_FelixReceiver_generator
# )

# cet_make_exec(test_tpf
#   SOURCE test_tpf.cpp
#   LIBRARIES ${TP_LIBS}
# )

# cet_make_exec(test_tpf_data_from_file
#   SOURCE test_tpf_data_from_file.cpp
#   LIBRARIES ${TP_LIBS}
# )

cet_make_exec(benchmark_tpf_rate
  SOURCE benchmark_tpf_rate.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(run_tpf_on_link
  SOURCE run_tpf_on_link.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(benchmark_no_expand
  SOURCE benchmark_no_expand.cpp
  LIBRARIES ${TP_LIBS}
)


cet_make_exec(dump_link
  SOURCE dump_link.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(dump_link_raw
  SOURCE dump_link_raw.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(subscribe_only
  SOURCE subscribe_only.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(read_raw_file
  SOURCE read_raw_file.cpp
  LIBRARIES ${TP_LIBS}
)

cet_make_exec(test_expand_segment_collection
  SOURCE test_expand_segment_collection.cpp
  LIBRARIES ${TP_LIBS} ${LIBZMQ}
)

cet_make_exec(check_output
  SOURCE check_output.cpp
  LIBRARIES ${TP_LIBS} ${LIBZMQ}
)
