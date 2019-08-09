cet_make_exec(test_PowerTwoHist
  SOURCE test_PowerTwoHist.cxx
)

cet_make_exec(check_latency
  SOURCE check_latency.cc
  LIBRARIES ${LIBPTMP} ${LIBCZMQ}
)

cet_script(check_many_latency.sh)
