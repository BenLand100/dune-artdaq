#define TRACE_NAME "StatusPublisher"
#include "trace.h"

#include "StatusPublisher.hh"
#include "fhiclcpp/fwd.h"
#include <sstream>

artdaq::StatusPublisher::StatusPublisher(std::string name,
					 std::string address,
					 uint8_t tracelvl)
//artdaq::StatusPublisher::StatusPublisher(fhicl::ParameterSet ps)
{
  //fProcessName = ps.get<std::string>("ProcessName");
  //fPublisherAddress = ps.get<std::string>("PublisherAddress");
  //fTRACELVL = ps.get<uint8_t>("TRACELVL",60);

  fProcessName = name;
  fPublisherAddress = address;
  fTRACELVL = tracelvl;

  fZMQContextPtr = zmq_ctx_new ();
  fZMQPublisherPtr = zmq_socket (fZMQContextPtr, ZMQ_PUB);
  /*  
  TLOG_ARB(fTRACELVL,TRACE_NAME) 
    << "Instantiated StatusPublisher: "
    << fProcessName << " "
    << "at " << fPublisherAddress
    << TLOG_ENDL;
  */
}

artdaq::StatusPublisher::~StatusPublisher()
{
  zmq_close(fZMQPublisherPtr);
  zmq_ctx_destroy(fZMQContextPtr);
  TRACE(fTRACELVL,"StatusPublisher %s destroyed",fProcessName.c_str());
}

int artdaq::StatusPublisher::BindPublisher()
{
  int rc = zmq_bind(fZMQPublisherPtr,fPublisherAddress.c_str());
  /*
  TLOG_ARB(fTRACELVL,TRACE_NAME)
    << "StatusPublisher " << fProcessName
    << " bound at " << fPublisherAddress
    << " (error code=" << rc << ")" << TLOG_ENDL;
  */
  return rc;
}

void artdaq::StatusPublisher::PublishStatus(const char* status,
					    std::string marker)
{
  gettimeofday(&fTimeStamp,NULL);
  std::stringstream ss;
  ss << STATUS_MSG_MARKER << "_" 
     << fProcessName << "_"
     << marker.c_str() << "_"
     << status << "_"
     << fTimeStamp.tv_sec << "." << fTimeStamp.tv_usec;

  zmq_send(fZMQPublisherPtr,ss.str().c_str(),ss.str().size(),0);
  /*
  TLOG_ARB(fTRACELVL,TRACE_NAME)
    << "StatusPublisher " << fProcessName
    << " send: " << ss.string()
    << TLOG_ENDL;
  */
}

void artdaq::StatusPublisher::PublishBadStatus(std::string marker)
{
  PublishStatus(STATUS_BAD,marker);
}
void artdaq::StatusPublisher::PublishGoodStatus(std::string marker)
{
  PublishStatus(STATUS_GOOD,marker);
}
