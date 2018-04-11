#ifndef IM_STATUSPUBLISHER
#define IM_STATUSPUBLISHER 1

#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>

#include "fhiclcpp/fwd.h"

#include "InhibitMasterDefinitions.h"


namespace artdaq {

  class StatusPublisher{

  public:
    //StatusPublisher(fhicl::ParameterSet);
    StatusPublisher(std::string,std::string,uint8_t tracelvl=60);
    ~StatusPublisher();

    int  BindPublisher();

    void PublishGoodStatus(std::string marker="*");
    void PublishBadStatus(std::string marker="*");
    
  private:
    
    void *fZMQContextPtr;      // = zmq_ctx_new ();
    void *fZMQPublisherPtr;    // = zmq_socket (context, ZMQ_PUB);

    std::string fProcessName;
    std::string fPublisherAddress;
    uint8_t     fTRACELVL;

    timeval fTimeStamp;

    void PublishStatus(const char*,std::string);
  };

}//endif artdaq

#endif
