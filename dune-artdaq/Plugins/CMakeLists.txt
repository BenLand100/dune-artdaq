
# JCF, 4/2/15

# The following search for the 0MQ library works on the gateway node;
# it is not guaranteed, however, that it will work on other systems

if (NOT ZEROMQ_IGNORED)

   FIND_LIBRARY(ZEROMQLIB zmq /usr/lib64)
   include_directories(/usr/include)

   simple_plugin("RCReporter" "metric"
     ${FHICLCPP}
     ${CETLIB}
     dune-artdaq_I3JSON
     dune-artdaq_RCConnection
   )

endif()
