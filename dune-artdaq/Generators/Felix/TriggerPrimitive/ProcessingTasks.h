#ifndef PROCESSINGTASKS_H
#define PROCESSINGTASKS_H

#include "zmq.h"
#include "dune-artdaq/Generators/Felix/NetioWIBRecords.hh"
#include <chrono>

// ProcessingTasks.h
// Author: Philip Rodrigues
//
// Some classes to facilitate telling the hit finding processing
// thread about new netio messages that are ready for processing. The
// main thread needs to return as soon as possible, so it just sends a
// message to the processing thread with the timestamp of and pointer
// to the netio message that's ready for processing. We use zeromq to
// communicate between threads, which has the nice effect that it does
// all the queueing for us


// The details of a netio message to be processed
struct ItemToProcess
{
    uint64_t timestamp;          // The timestamp of the first frame in the
                                 // netio message
    SUPERCHUNK_CHAR_STRUCT* scs; // Pointer to the (raw) message to be
                                 // processed
    uint64_t timeQueued;         // The time this item was queued by
                                 // ItemPublisher, so receivers can detect
                                 // whether they're getting behind
};

// Class to send out details of netio messages to be processed
class ItemPublisher
{
public:

    static constexpr uint64_t END_OF_MESSAGES=0;

    // Return the current steady clock in microseconds
    static uint64_t now_us()
    {
        // std::chrono is the worst
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // Construct an ItemPublisher given a ZeroMQ context
    ItemPublisher(void* context)
    {
        m_publishSocket=zmq_socket(context, ZMQ_PUB);
        if(!m_publishSocket){
            std::cout << "Error creating publish socket" << std::endl;
            return;
        }
        // We set the high-water mark to infinity, so no messages are
        // ever dropped. It's the receiver's responsibility to skip
        // messages if it gets behind
        int sndhwm=0;
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
        ItemToProcess item{timestamp, scs, now_us()};
        return zmq_send(m_publishSocket, &item, sizeof(ItemToProcess), 0);
    }

    int publishStop()
    {
        return publishItem(END_OF_MESSAGES, nullptr);
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
    // Construct an item receiver given a zeromq context (must be the same context that was given to the ItemPublisher we want to get tasks from)
    ItemReceiver(void* context)
    {
        m_subscribeSocket=zmq_socket(context, ZMQ_SUB);
        int rc=zmq_connect(m_subscribeSocket, "inproc://messages");
        if(rc!=0) std::cout << "Error connecting subscriber" << std::endl;
        rc=zmq_setsockopt(m_subscribeSocket, ZMQ_SUBSCRIBE,
                          NULL, 0); // Subscribe to all messages
        if(rc!=0) std::cout << "Error subscribing to tag" << std::endl;
        // We set the high-water mark to infinity, so no messages are
        // ever dropped. Caller has to decide based on the queueing
        // times whether it's getting behind and what to do about it
        int rcvhwm=0;
        rc=zmq_setsockopt(m_subscribeSocket, ZMQ_RCVHWM,
                          &rcvhwm, sizeof(int)); // Infinite HWM
        if(rc!=0) std::cout << "Error setting rcv HWM" << std::endl;
    }

    ~ItemReceiver()
    {
    }

    // Receive an item, which is returned. `should_stop` is set to
    // true if the item is a special "end-of-messages" item
    ItemToProcess recvItem(bool& should_stop)
    {
        ItemToProcess item;
        zmq_recv(m_subscribeSocket, &item, sizeof(ItemToProcess), 0);
        should_stop=(item.timestamp==ItemPublisher::END_OF_MESSAGES);
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

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
