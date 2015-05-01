
# JCF, 4/2/15

# The following search for the 0MQ library works on the gateway node;
# it is not guaranteed, however, that it will work on other systems

FIND_LIBRARY(ZEROMQLIB zmq /usr/lib64)
include_directories(/usr/include)

simple_plugin("RCReporter" "metric"
  ${FHICLCPP}
  ${CETLIB}
  lbne-artdaq_I3JSON
  lbne-artdaq_RCConnection
)
