#include "trace.h"

#include "HwClockSubscriber.hh"

namespace dune {

// ----------------------------------------------------------------------------
HwClockSubscriber::HwClockSubscriber( const std::string& aAddress ) :
    address_(aAddress) {

    context_ = zmq_ctx_new();
    socket_ = zmq_socket(context_, ZMQ_SUB);

}


// ----------------------------------------------------------------------------
HwClockSubscriber::~HwClockSubscriber() {
    zmq_close(socket_);
    zmq_ctx_destroy(context_);
}


// ----------------------------------------------------------------------------
int HwClockSubscriber::connect( int timeout ) {
    int rc = zmq_connect(socket_, address_.c_str());
    zmq_setsockopt(socket_, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
    return rc;
}


// ----------------------------------------------------------------------------
uint64_t HwClockSubscriber::receiveHardwareTimeStamp() {

    uint64_t ts(0);
    int rc = zmq_recv(socket_, &ts, sizeof(ts), 0);
    if(rc==-1) 
        return 0;
    return ts;

}

} // namespace dune
