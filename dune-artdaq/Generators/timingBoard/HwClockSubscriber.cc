#include "dune-artdaq/DAQLogger/DAQLogger.hh"
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
    if (rc!=0){
      dune::DAQLogger::LogWarning("HwClockSubscriber::start") << "ZMQ connect return code is not zero, but: " << rc;
    }
    else{
      dune::DAQLogger::LogInfo("HwClockSubscriber::start") << "Connected successfully to ZMQ Socket: " << address_ ;
    }
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
