#cet_make_exec(test_PowerTwoHist
#  SOURCE test_PowerTwoHist.cxx
#)

#cet_make_exec(check_latency
#  SOURCE check_latency.cc
#  LIBRARIES ${PTMP_LIBRARIES}
#)

cet_script(check_many_latency.sh)

art_make_library( LIBRARY_NAME fhicl-to-json
		  SOURCE fhicl-to-json.cpp
                  LIBRARIES ${FHICLCPP} ${CETLIB}
)
