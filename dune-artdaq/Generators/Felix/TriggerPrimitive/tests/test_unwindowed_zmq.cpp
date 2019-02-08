#include <thread>
#include <atomic>
#include "zmq.h"

#include "FrameFile.h"
#include "../TriggerPrimitiveFinder.h"
#include "../process_avx2.h"

std::atomic<uint64_t> last_ts_processed{0};
std::mutex hit_queue_mutex;

struct ItemToProcess
{
    uint64_t timestamp;
    SUPERCHUNK_CHAR_STRUCT* scs;
};

//==============================================================================
void addHitsToQueue(uint64_t timestamp,
                    const uint16_t* input_loc,
                    std::deque<dune::TriggerPrimitive>& primitive_queue)
{
    uint16_t chan[16], hit_start[16], hit_charge[16], hit_tover[16];

    std::lock_guard<std::mutex> guard(hit_queue_mutex);

    while(*input_loc!=MAGIC){
        // for(int i=0; i<16; ++i) chan[i]       = m_offlineChannelOffset+collection_index_to_offline(*input_loc++);
        for(int i=0; i<16; ++i) chan[i]       = *input_loc++;
        for(int i=0; i<16; ++i) hit_start[i]  = *input_loc++;
        for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
        for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;
        
        for(int i=0; i<16; ++i){
            if(hit_charge[i] && chan[i]!=MAGIC){
                primitive_queue.emplace_back(chan[i], timestamp+hit_start[i], hit_charge[i], hit_tover[i]);
            }
        }
    }
    while(primitive_queue.size()>10000) primitive_queue.pop_front();
}

//============================================================================== 
std::vector<dune::TriggerPrimitive> getHitsForWindow(const std::deque<dune::TriggerPrimitive>& primitive_queue,
                                                     uint64_t start_ts, uint64_t end_ts)
{
    // Wait for the processing to catch up
    while(last_ts_processed.load()<end_ts){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    std::vector<dune::TriggerPrimitive> ret;
    ret.reserve(5000);

    {
        std::lock_guard<std::mutex> guard(hit_queue_mutex);
        for(auto const& prim: primitive_queue){
            if(prim.startTime>start_ts && prim.startTime<end_ts){
                ret.push_back(prim);
            }
            // The trigger primitives are ordered by
            // timestamp, so as soon as we've seen one that's too
            // late, we're done
            if(prim.startTime>end_ts){
                break;
            }
        }
    }
    return ret;
}

//==============================================================================
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

    // Temporary place to stash the hits
    uint16_t* primfind_dest=new uint16_t[100000];
    
    // Place to put the hits more tidily for later retrieval
    std::deque<dune::TriggerPrimitive> primitive_queue;
      
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
    bool first=true;
    while(true){
        ItemToProcess item;
        zmq_recv(subscriber, &item, sizeof(ItemToProcess), 0);
        ++nmsg;
        // A value of zero means "end of messages"
        if(!item.scs) break;
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*item.scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        if(first){
            pi.setState(mcadc);
            first=false;
        }
        pi.input=mcadc;
        // "Empty" the list of hits
        *primfind_dest=MAGIC;
        // Do the processing
        process_window_avx2(pi);
        // Create dune::TriggerPrimitives from the hits and put them in the queue for later retrieval
        addHitsToQueue(item.timestamp, primfind_dest, primitive_queue);
        last_ts_processed.store(item.timestamp);
    }
    std::cout << "Received " << nmsg << " messages. Found " << pi.nhits << " hits" << std::endl;

    // -------------------------------------------------------- 
    // Cleanup

    delete primfind_dest;
    zmq_close(subscriber);
}

//==============================================================================
int main()
{
    // -------------------------------------------------------- 
    // Set up the ZeroMQ socket on which we'll send new tasks

    void* context=zmq_ctx_new();
    void* publisher=zmq_socket(context, ZMQ_PUB);
    int sndhwm=0;
    int rc=zmq_setsockopt(publisher, ZMQ_SNDHWM,
                          &sndhwm, sizeof(int));
    if(rc!=0) std::cout << "Error setting send HWM" << std::endl;
    rc=zmq_bind(publisher, "inproc://messages");
    if(rc!=0) std::cout << "Error binding publisher" << std::endl;

    // -------------------------------------------------------- 
    // Set the processing thread going

    std::thread t1(processing_thread, context, 0, REGISTERS_PER_FRAME);

    pthread_setname_np(pthread_self(), "main");

    // Wait for the zeromq socket to bind
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // -------------------------------------------------------- 
    // Actually send the messages

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
            // The first frame in the message
            FelixFrame* frame=reinterpret_cast<FelixFrame*>(fragment+imessage*NETIO_MSG_SIZE);
            ItemToProcess item{frame->timestamp(), scs};
            zmq_send(publisher, &item, sizeof(ItemToProcess), 0);
            // std::this_thread::sleep_for(std::chrono::microseconds(1024));
        }
    }
    
    // Send 0 as an "end-of-stream" message
    ItemToProcess item{0, 0};
    zmq_send(publisher, &item, sizeof(ItemToProcess), 0);

    // -------------------------------------------------------- 
    // Wait for the processing to finish
    t1.join();

    // -------------------------------------------------------- 
    // Print summary and cleanup
    auto time1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(time1-time0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;

    zmq_close(publisher);
    zmq_ctx_destroy(context);
}
