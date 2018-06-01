#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <iostream>

#include "stdint.h"

bool rcvMore(void* socket)
{
    int rcvmore;
    size_t option_len;
    zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore, &option_len);
    return rcvmore;
}

uint32_t getVal(void* socket, bool needMore)
{
    uint32_t val;
    zmq_recv(socket, &val, sizeof(val), 0);
    if(needMore) assert(rcvMore(socket));
    return val;
}

int main(int argc, char** argv)
{
    if(argc!=2){
        std::cout << "Usage: FragmentSubscriber connection_string" << std::endl;
    }

    void* ctx = zmq_ctx_new();
    void* socket = zmq_socket(ctx, ZMQ_SUB);
    zmq_connect(socket, argv[1]);
    
    zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0);
    
    uint32_t cookie  = getVal(socket, true);
    uint32_t scmd    = getVal(socket, true);
    uint32_t tcmd    = getVal(socket, true);
    uint32_t tstampl = getVal(socket, true);
    uint32_t tstamph = getVal(socket, true);
    uint32_t evtctr  = getVal(socket, true);
    uint32_t cksum   = getVal(socket, false);
    
    std::cout << "Got fragment: " << std::endl
              << "  cookie  = " << cookie  << std::endl
              << "  scmd    = " << scmd    << std::endl
              << "  tcmd    = " << tcmd    << std::endl
              << "  tstampl = " << tstampl << std::endl
              << "  tstamph = " << tstamph << std::endl
              << "  evtctr  = " << evtctr  << std::endl
              << "  cksum   = " << cksum   << std::endl;

    zmq_close(socket);
    zmq_ctx_destroy(ctx);
}
