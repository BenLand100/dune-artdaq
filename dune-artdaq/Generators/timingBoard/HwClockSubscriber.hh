#ifndef __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HWCLOCKSUBSCRIBER_HH__
#define __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HWCLOCKSUBSCRIBER_HH__

#include "zmq.h"
#include <unistd.h>
#include <string>


namespace dune {

class HwClockSubscriber {

  public:
    HwClockSubscriber(const std::string& aAddress);
    ~HwClockSubscriber();

    int connect( int timeout=-1 );
    uint64_t receiveHardwareTimeStamp();

  private:

    void *context_;      // = zmq_ctx_new ();
    void *socket_;    // = zmq_socket (context, ZMQ_PUB);

    const std::string address_;
};

}

#endif /* __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HARDWARETIMESUBSCRIBER_HH__ */
