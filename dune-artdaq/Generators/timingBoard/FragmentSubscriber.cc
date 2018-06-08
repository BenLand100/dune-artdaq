#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>

#include "stdint.h"

// Are there more message parts waiting on the socket?
bool rcvMore(void* socket)
{
    int rcvmore;
    size_t option_len;
    zmq_getsockopt(socket, ZMQ_RCVMORE, &rcvmore, &option_len);
    return rcvmore;
}

std::vector<uint64_t> getVals(void* socket)
{
    std::vector<uint64_t> ret;
    uint64_t val;
    int rc=zmq_recv(socket, &val, sizeof(val), ZMQ_DONTWAIT);
    // Did we get anything on the socket?
    if(rc==-1 && errno==EAGAIN){
        // No, so return empty vector
        return ret;
    }
    else if(rc==-1){
        // Some other error. Print it and return empty vector
        std::cout << "Error in zmq_recv(): " << strerror(errno) << std::endl;
        return ret;
    }
    else{
        // Yes, so receive the whole message
        ret.push_back(val);
        while(rcvMore(socket)){
            zmq_recv(socket, &val, sizeof(val), 0);
            ret.push_back(val);
        }
        return ret;
    }
}

int main(int argc, char** argv)
{
    // connection_string should be compatible with the
    // zmq_fragment_connection_out fcl parameter in the board reader
    // (but with any "*" replaced with the machine name that the board
    // reader is running on)
    if(argc!=2){
        std::cout << "Usage: FragmentSubscriber connection_string" << std::endl;
    }

    // Create the zmq context and socket
    void* ctx = zmq_ctx_new();
    void* socket = zmq_socket(ctx, ZMQ_SUB);

    // Connect the socket to the other end, and subscribe to all the messages on it
    zmq_connect(socket, argv[1]);
    zmq_setsockopt(socket, ZMQ_SUBSCRIBE, "", 0);

    // Loop for 10 seconds reading messages from the socket
    auto startTime=std::chrono::system_clock::now();

    while(true){
        auto now=std::chrono::system_clock::now();
        if(now-startTime > std::chrono::seconds(10)){
            std::cout << "10 seconds up. Done" << std::endl;
            break;
        }

        // Get the list of values from the socket. Returns empty vector if nothing available
        std::vector<uint64_t> vals=getVals(socket);

        if(vals.empty()){
            // There was no message waiting.
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        else{
            std::cout << "Got fragment: " << std::endl;
            std::cout << "sequenceID :" << vals[0] << std::endl;
            std::cout << "fragmentID :" << vals[1] << std::endl;
            std::cout << "cookie :"     << vals[2] << std::endl;
            std::cout << "scmd :"       << vals[3] << std::endl;
            std::cout << "tcmd :"       << vals[4] << std::endl;
            std::cout << "tstamp :"     << vals[5] << std::endl;
            std::cout << "evtctr :"     << vals[6] << std::endl;
            std::cout << "cksum :"      << vals[7] << std::endl;
        }
    }

    zmq_close(socket);
    zmq_ctx_destroy(ctx);
}
