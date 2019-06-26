cet_make_exec(test_PowerTwoHist
  SOURCE test_PowerTwoHist.cxx
)

art_make_library( LIBRARY_NAME dune-artdaq_Generators_swTrigger
       SOURCE AdjacencyAlgorithms.cpp TriggerCandidate.cpp ModuleTrigger.cpp
)

