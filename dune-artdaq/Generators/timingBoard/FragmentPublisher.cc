#include "trace.h"

#include "FragmentPublisher.hh"
#include "fhiclcpp/fwd.h"
#include <sstream>

artdaq::FragmentPublisher::FragmentPublisher(std::string address)
{
    fPublisherAddress = address;
    fZMQContextPtr = zmq_ctx_new();
    fZMQPublisherPtr = zmq_socket(fZMQContextPtr, ZMQ_PUB);
}

artdaq::FragmentPublisher::~FragmentPublisher()
{
    zmq_close(fZMQPublisherPtr);
    zmq_ctx_destroy(fZMQContextPtr);
}

int artdaq::FragmentPublisher::BindPublisher()
{
    int rc = zmq_bind(fZMQPublisherPtr, fPublisherAddress.c_str());
    return rc;
}

bool artdaq::FragmentPublisher::SendVal(uint32_t val, bool sendmore)
{
    int rc = zmq_send(fZMQPublisherPtr, &val,  sizeof(uint32_t), sendmore ?  ZMQ_SNDMORE : 0);
    return rc!=-1;
}

bool artdaq::FragmentPublisher::PublishFragment(dune::TimingFragment* frag)
{
    bool success=true;
    success = success && SendVal(frag->get_cookie(),  true);
    success = success && SendVal(frag->get_scmd(),    true);
    success = success && SendVal(frag->get_tcmd(),    true);
    success = success && SendVal(frag->get_tstampl(), true);
    success = success && SendVal(frag->get_tstamph(), true);
    success = success && SendVal(frag->get_evtctr(),  true);
    success = success && SendVal(frag->get_cksum(),   false);

    return success;
}
