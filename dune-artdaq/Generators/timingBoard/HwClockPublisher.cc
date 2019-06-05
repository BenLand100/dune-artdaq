#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "trace.h"

#include "HwClockPublisher.hh"

namespace artdaq {

// ----------------------------------------------------------------------------
HwClockPublisher::HwClockPublisher( const std::string& aAddress ) :
    address_(aAddress) {

    context_ = zmq_ctx_new();
    socket_ = zmq_socket(context_, ZMQ_PUB);

}


// ----------------------------------------------------------------------------
HwClockPublisher::~HwClockPublisher() {
    zmq_close(socket_);
    zmq_ctx_destroy(context_);
}


// ----------------------------------------------------------------------------
int HwClockPublisher::bind() {
    int rc = zmq_bind(socket_, address_.c_str());
    if (rc!=0){
      dune::DAQLogger::LogWarning("HwClockPublisher::start") << "ZMQ connect return code is not zero, but: " << rc;
    }
    else{
      dune::DAQLogger::LogInfo("HwClockPublisher::start") << "Connected successfully to ZMQ Socket: " << address_ ;
    }

    return rc;
}


// ----------------------------------------------------------------------------
bool HwClockPublisher::publish(uint64_t ts) {

    bool lSuccess = true;
    lSuccess &= this->SendVal(ts, false);

    return lSuccess;
}

// ----------------------------------------------------------------------------
bool HwClockPublisher::SendVal(uint64_t val, bool sendmore) {
    int rc = zmq_send(socket_, &val,  sizeof(val), sendmore ?  ZMQ_SNDMORE : 0);
    return rc != -1;
}
} // namespace artdaq
