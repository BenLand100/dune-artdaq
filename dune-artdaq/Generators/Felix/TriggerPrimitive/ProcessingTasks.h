#ifndef PROCESSINGTASKS_H
#define PROCESSINGTASKS_H

#include "zmq.h"
#include "dune-artdaq/Generators/Felix/NetioWIBRecords.hh"

struct ItemToProcess
{
    uint64_t timestamp;
    SUPERCHUNK_CHAR_STRUCT* scs;
};

class ItemPublisher
{
public:
    ItemPublisher(void* context)
    {
        m_publishSocket=zmq_socket(context, ZMQ_PUB);
        if(!m_publishSocket){
            std::cout << "Error creating publish socket" << std::endl;
            return;
        }
        int sndhwm=100;
        int rc=zmq_setsockopt(m_publishSocket, ZMQ_SNDHWM,
                              &sndhwm, sizeof(int));
        if(rc!=0){
            std::cout << "Error setting publish HWM" << std::endl;
            return;
        }
        
        rc=zmq_bind(m_publishSocket, "inproc://messages");
        if(rc!=0) std::cout << "Error binding publisher" << std::endl;
    }


    ~ItemPublisher()
    {
    }

    int publishItem(uint64_t timestamp, SUPERCHUNK_CHAR_STRUCT* scs)
    {
        ItemToProcess item{timestamp, scs};
        return zmq_send(m_publishSocket, &item, sizeof(ItemToProcess), 0);
    }

    int publishStop()
    {
        return publishItem(0, nullptr);
    }

    void closeSocket()
    {
        // close socket
        zmq_close(m_publishSocket);   
    }

private:
    void* m_publishSocket;
};

class ItemReceiver
{
public:
    ItemReceiver(void* context)
    {
        m_subscribeSocket=zmq_socket(context, ZMQ_SUB);
        int rc=zmq_connect(m_subscribeSocket, "inproc://messages");
        if(rc!=0) std::cout << "Error connecting subscriber" << std::endl;
        rc=zmq_setsockopt(m_subscribeSocket, ZMQ_SUBSCRIBE,
                          NULL, 0); // Subscribe to all messages
        if(rc!=0) std::cout << "Error subscribing to tag" << std::endl;
        int rcvhwm=100;
        rc=zmq_setsockopt(m_subscribeSocket, ZMQ_RCVHWM,
                          &rcvhwm, sizeof(int)); // Infinite HWM
        if(rc!=0) std::cout << "Error setting rcv HWM" << std::endl;
    }

    ~ItemReceiver()
    {
    }

    ItemToProcess recvItem(bool& should_stop)
    {
        ItemToProcess item;
        zmq_recv(m_subscribeSocket, &item, sizeof(ItemToProcess), 0);
        should_stop=(item.timestamp==0);
        return item;
    }

    void closeSocket()
    {
        // close socket
        zmq_close(m_subscribeSocket);   
    }

private:
    void* m_subscribeSocket;
};

#endif // include guard
