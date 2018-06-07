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

bool artdaq::FragmentPublisher::SendVal(uint64_t val, bool sendmore)
{
    int rc = zmq_send(fZMQPublisherPtr, &val,  sizeof(val), sendmore ?  ZMQ_SNDMORE : 0);
    return rc!=-1;
}

bool artdaq::FragmentPublisher::PublishFragment(artdaq::Fragment* frag, dune::TimingFragment* timingFrag)
{
    bool success=true;
    success = success && SendVal(frag->sequenceID(),        true);
    success = success && SendVal(frag->fragmentID(),        true);
    success = success && SendVal(timingFrag->get_cookie(),  true);
    success = success && SendVal(timingFrag->get_scmd(),    true);
    success = success && SendVal(timingFrag->get_tcmd(),    true);
    success = success && SendVal(timingFrag->get_tstamp(),  true);
    success = success && SendVal(timingFrag->get_evtctr(),  true);
    success = success && SendVal(timingFrag->get_cksum(),   false);

    return success;
}
