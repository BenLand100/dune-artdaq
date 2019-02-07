#include <thread>
#include <atomic>
#include "zmq.h"

#include "FrameFile.h"
#include "../TriggerPrimitiveFinder.h"
#include "../process_avx2.h"

void processing_thread(void* context, uint8_t first_register, uint8_t last_register)
{
    pthread_setname_np(pthread_self(), "processing");

    // -------------------------------------------------------- 
    // Set up the ZeroMQ socket on which we'll listen for new tasks

    void* subscriber=zmq_socket(context, ZMQ_SUB);
    int rc=zmq_connect(subscriber, "inproc://messages");
    if(rc!=0) std::cout << "Error connecting subscriber" << std::endl;
    rc=zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE,
                      NULL, 0); // Subscribe to all messages
    if(rc!=0) std::cout << "Error subscribing to tag" << std::endl;
    int rcvhwm=0;
    rc=zmq_setsockopt(subscriber, ZMQ_RCVHWM,
                      &rcvhwm, sizeof(int)); // Infinite HWM
    if(rc!=0) std::cout << "Error setting rcv HWM" << std::endl;

    // -------------------------------------------------------- 
    // Set up the processing info
    
    const uint8_t tap_exponent=6;
    const int multiplier=1<<tap_exponent; // 64
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two

    // TODO: ProcessingInfo points to but doesn't copy the taps it's
    // passed, so we make a long-lived copy here. We ought to not leak
    // this (and also do it properly)
    int16_t* taps_p=new int16_t[taps.size()];
    for(size_t i=0; i<taps.size(); ++i) taps_p[i]=taps[i];

    uint16_t* primfind_dest=new uint16_t[100000];
      
    ProcessingInfo pi(nullptr,
                      FRAMES_PER_MSG, // We'll just process one message
                      first_register, // First register
                      last_register, // Last register
                      primfind_dest,
                      taps_p, (uint8_t)taps.size(),
                      tap_exponent,
                      0);
    
    // -------------------------------------------------------- 
    // Actually process
    int nmsg=0;
    int nhits=0;
    bool first=true;
    while(true){
        SUPERCHUNK_CHAR_STRUCT* scs=0;
        zmq_recv(subscriber, &scs, sizeof(scs), 0);
        ++nmsg;
        // std::cout << "Got 0x" << std::hex << (void*)scs << std::dec << std::endl;
        if(!scs) break;
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        if(first){
            pi.setState(mcadc);
            first=false;
        }
        pi.input=mcadc;
        pi=process_window_avx2(pi);
        nhits+=pi.nhits;
    }
    std::cout << "Received " << nmsg << " messages. Found " << nhits << " hits" << std::endl;

    delete primfind_dest;

    zmq_close(subscriber);
}

int main()
{
    void* context=zmq_ctx_new();
    void* publisher=zmq_socket(context, ZMQ_PUB);
    int sndhwm=0;
    int rc=zmq_setsockopt(publisher, ZMQ_SNDHWM,
                          &sndhwm, sizeof(int));
    if(rc!=0) std::cout << "Error setting send HWM" << std::endl;
    rc=zmq_bind(publisher, "inproc://messages");
    if(rc!=0) std::cout << "Error binding publisher" << std::endl;

    std::thread t1(processing_thread, context, 0,                     REGISTERS_PER_FRAME/2);
    std::thread t2(processing_thread, context, REGISTERS_PER_FRAME/2, REGISTERS_PER_FRAME);

    pthread_setname_np(pthread_self(), "main");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;
    const int n_repeats=1000;
    auto time0=std::chrono::steady_clock::now();

    std::cout << "Start sending messages..." << std::endl;
    for(int irep=0; irep<n_repeats; ++irep){
        for(size_t imessage=0; imessage<n_messages; ++imessage){
            SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
            zmq_send(publisher, &scs, sizeof(scs), 0);
            // std::this_thread::sleep_for(std::chrono::microseconds(1024));
        }
    }
    
    // Send 0 as an "end-of-stream" message
    SUPERCHUNK_CHAR_STRUCT* scs=0;
    zmq_send(publisher, &scs, sizeof(scs), 0);

    t1.join();
    t2.join();

    auto time1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(time1-time0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;

    zmq_close(publisher);
    zmq_ctx_destroy(context);
}
