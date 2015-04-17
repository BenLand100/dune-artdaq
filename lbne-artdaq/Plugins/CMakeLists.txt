
# JCF, 4/2/15

# The following search for the 0MQ library works on the gateway node;
# it is not guaranteed, however, that it will work on other systems

FIND_LIBRARY(ZEROMQLIB zmq /usr/lib64)
include_directories(/usr/include)

art_make_library( LIBRARY_NAME lbne-artdaq_I3JSON
		  SOURCE
		  I3JSON.cc
        LIBRARIES
        ${CETLIB}
        )



simple_plugin("rcreporter" "metric"
  ${FHICLCPP}
  ${CETLIB}
  ${ZEROMQLIB}
  lbne-artdaq_I3JSON
)
