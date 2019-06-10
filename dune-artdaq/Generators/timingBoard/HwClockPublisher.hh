#ifndef __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HWCLOCKPUBLISHER_HH__
#define __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HWCLOCKPUBLISHER_HH__

#include "zmq.h"
#include <unistd.h>
#include <string>


namespace artdaq {

class HwClockPublisher {

  public:
    HwClockPublisher(const std::string& aAddress);
    ~HwClockPublisher();

    int  bind();
    bool publish(uint64_t ts);

  private:
    bool SendVal(uint64_t val, bool sendmore);

    void *context_;
    void *socket_;

    const std::string address_;
};

}

#endif /* __DUNE_ARTDAQ_GENERATORS_TIMINGBOARD_HARDWARETIMEPUBLISHER_HH__ */
