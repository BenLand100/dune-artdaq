#ifndef IM_FRAGMENTPUBLISHER
#define IM_FRAGMENTPUBLISHER 1

#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>


#include "fhiclcpp/fwd.h"

#include "dune-raw-data/Overlays/TimingFragment.hh"

namespace artdaq {

  class FragmentPublisher {

  public:
    FragmentPublisher(std::string);
    ~FragmentPublisher();

    int  BindPublisher();

    bool PublishFragment(artdaq::Fragment* frag, dune::TimingFragment* timingFrag);
    
  private:
    
    bool SendVal(uint64_t val, bool sendmore);

    void *fZMQContextPtr;      // = zmq_ctx_new ();
    void *fZMQPublisherPtr;    // = zmq_socket (context, ZMQ_PUB);

    std::string fPublisherAddress;
    uint8_t     fTRACELVL;

  };

}//end namespace artdaq

#endif
