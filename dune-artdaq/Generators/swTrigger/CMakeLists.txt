cet_make_exec(test_PowerTwoHist
  SOURCE test_PowerTwoHist.cxx
)

art_make_library( LIBRARY_NAME dune-artdaq_Generators_swTrigger
       SOURCE AdjacencyAlgorithms.cpp TriggerCandidate.cpp ModuleTrigger.cpp
)

cet_make_exec(check_latency
  SOURCE check_latency.cc
  LIBRARIES ${LIBPTMP} ${LIBCZMQ}
)

cet_script(check_many_latency.sh)