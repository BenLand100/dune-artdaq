#include "TriggerPrimitiveFinder.h"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"

#include <cstddef> // For offsetof

const int64_t clocksPerTPCTick=25;

//======================================================================
TriggerPrimitiveFinder::TriggerPrimitiveFinder(int32_t cpu_offset, int item_queue_size)
    : m_readyForMessages(false),
      m_itemsToProcess(item_queue_size),
      m_fiber_no(0xff),
      m_slot_no(0xff),
      m_crate_no(0xff),
      m_TPSender("{\"socket\": { \"type\": \"PUB\", \"bind\": [ \"tcp://127.0.0.1:6789\" ] } }")
{
    m_processingThread=std::thread(&TriggerPrimitiveFinder::processing_thread, this, 0, REGISTERS_PER_FRAME);
    if(cpu_offset>=0){
        // Copied from NetioHandler::lockSubsToCPUs()
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        unsigned short cpuid = cpu_offset;
        CPU_SET(cpuid, &cpuset);    // The physical core
        CPU_SET(cpuid+24, &cpuset); // The corresponding HT core
        pthread_setaffinity_np(m_processingThread.native_handle(), sizeof(cpu_set_t), &cpuset);
    }
}

//======================================================================
TriggerPrimitiveFinder::~TriggerPrimitiveFinder()
{
    m_itemsToProcess.write(ProcessingTasks::ItemToProcess{ProcessingTasks::END_OF_MESSAGES, nullptr, ProcessingTasks::now_us()});
    m_processingThread.join(); // Wait for it to actually stop
}

//======================================================================
void TriggerPrimitiveFinder::addMessage(SUPERCHUNK_CHAR_STRUCT& ucs)
{
    if(m_readyForMessages.load()){
        // The first frame in the message
        dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(&ucs);
        m_itemsToProcess.write(ProcessingTasks::ItemToProcess{frame->timestamp(), &ucs, ProcessingTasks::now_us()});
    }
}

//======================================================================
void TriggerPrimitiveFinder::hitsToFragment(uint64_t timestamp, uint32_t window_size, artdaq::Fragment* fragPtr)
{
    std::vector<dune::TriggerPrimitive> tps=getHitsForWindow(m_triggerPrimitives,
                                                             timestamp, timestamp+window_size);
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::hitsToFragment") << "Got " << tps.size() << " hits for timestamp 0x" << std::hex << timestamp << std::dec;

    // The data payload of the fragment will be:
    // uint64_t timestamp
    // uint32_t nhits
    // N*TriggerPrimitive
    fragPtr->resizeBytes(sizeof(dune::FelixFragmentHits::Body)+tps.size()*sizeof(dune::TriggerPrimitive));
    dune::FelixFragmentHits hitFrag(*fragPtr);

    hitFrag.set_timestamp(timestamp);
    hitFrag.set_nhits(tps.size());

    hitFrag.set_fiber_no(m_fiber_no);
    hitFrag.set_slot_no(m_slot_no);
    hitFrag.set_crate_no(m_crate_no);

    std::cout << "Hits for timestamp " << timestamp << ", window size " << window_size << std::endl;
    for(size_t i=0; i<tps.size(); ++i){
        hitFrag.get_primitive(i)=tps[i];
        std::cout << hitFrag.get_primitive(i).startTime << " " << hitFrag.get_crate_no() << " " << hitFrag.get_slot_no() << " " << hitFrag.get_fiber_no() << " " << hitFrag.get_primitive(i).channel << std::endl;
    }
    std::cout << std::endl;
}

//======================================================================
std::vector<dune::TriggerPrimitive>
TriggerPrimitiveFinder::getHitsForWindow(const std::deque<dune::TriggerPrimitive>& primitive_queue,
                                         uint64_t start_ts, uint64_t end_ts)
{
    // Wait for the processing to catch up
    while(m_latestProcessedTimestamp.load()<end_ts){
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
    std::vector<dune::TriggerPrimitive> ret;
    ret.reserve(5000);

    {
        std::lock_guard<std::mutex> guard(m_triggerPrimitiveMutex);

        // The hits in the queue are ordered by time, so it might be faster to search with lower_bound() and upper_bound like this:
        // std::deque<int> queue{1,2,2,3,5,6,6,6,6,7,8,9,10,11,12,15,17,19};
        // int window_start=4;
        // int window_end=7;
        // auto const& itlo=std::lower_bound(queue.cbegin(), queue.cend(), window_start, [](const int& a, const int& b){ return a<b; });
        // auto const& itup=std::upper_bound(queue.cbegin(), queue.cend(), window_end,   [](const int& a, const int& b){ return a<b; });
        // std::cout << "Window [" << window_start << ", " << window_end << "] returned indices " << std::distance(queue.cbegin(), itlo) << " to " << std::distance(queue.cbegin(), itup) << std::endl;

        for(auto const& prim: primitive_queue){
            // startTime is in PDTS clock ticks, but timeOverThreshold is in TPC ticks
            uint64_t endTime=prim.startTime+clocksPerTPCTick*prim.timeOverThreshold;
            if(endTime>start_ts && endTime<end_ts){
                ret.push_back(prim);
            }
            // The trigger primitives are ordered by
            // *end* timestamp, so as soon as we've seen one that's too
            // late, we're done
            if(endTime>end_ts){
                break;
            }
        }
    }
    return ret;
}

//======================================================================
unsigned int
TriggerPrimitiveFinder::addHitsToQueue(uint64_t timestamp,
                                       const uint16_t* input_loc,
                                       std::deque<dune::TriggerPrimitive>& primitive_queue)
{
    uint16_t chan[16], hit_end[16], hit_charge[16], hit_tover[16];
    unsigned int nhits=0;
    std::lock_guard<std::mutex> guard(m_triggerPrimitiveMutex);
    // std::cout << std::endl << "Adding hits for ts 0x" << std::hex << timestamp << std::dec << std::endl;

    static uint32_t count=0;
    ptmp::data::TPSet tpset;
    tpset.set_count(count++);
    tpset.set_detid(4);

    while(*input_loc!=MAGIC){
        for(int i=0; i<16; ++i) chan[i]       = collection_index_to_channel(*input_loc++);
        //for(int i=0; i<16; ++i) chan[i]       = *input_loc++;
        for(int i=0; i<16; ++i) hit_end[i]    = *input_loc++;
        for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
        for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;
        
        for(int i=0; i<16; ++i){
            if(hit_charge[i] && chan[i]!=MAGIC){
                uint64_t hit_start=timestamp+clocksPerTPCTick*(int64_t(hit_end[i])-hit_tover[i]);
                // std::cout << "0x" << std::hex << hit_start << std::dec << " " << chan[i] << std::endl;
                primitive_queue.emplace_back(chan[i], hit_start, hit_charge[i], hit_tover[i]);
                ptmp::data::TrigPrim* ptmp_prim=tpset.add_tps();
                ptmp_prim->set_channel(chan[i]);
                ptmp::data::Timestamp* ts=ptmp_prim->mutable_tstart();
                ts->set_seconds(hit_start);
                ptmp_prim->set_tspan(hit_tover[i]);
                ptmp_prim->set_adcsum(hit_charge[i]);
                ++nhits;
            }
        }
    }
    // Send out the TPSet
    m_TPSender(tpset);
    while(primitive_queue.size()>10000) primitive_queue.pop_front();
    return nhits;
}

//======================================================================
void TriggerPrimitiveFinder::measure_latency(const ProcessingTasks::ItemToProcess& item)
{
    // All these statics mean this function can only be called from one thread, I assume
    static constexpr int latencyThresholdEnter=100000; // us
    static constexpr int latencyThresholdLeave=latencyThresholdEnter/2;
    static bool was_behind=false;
    static uint64_t entered_late_time=0;

    uint64_t now=ProcessingTasks::now_us();
    int64_t latency=now-item.timeQueued;
    static int64_t last_printed_latency=0;
    if(!was_behind && latency > latencyThresholdEnter){
        entered_late_time=now;
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing late by " << (latency/1000) << "ms (threshold is " << (latencyThresholdEnter/1000) << "ms)";
        was_behind=true;
    }
    if(latency < latencyThresholdLeave && was_behind){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing caught up. Was late for " << ((now-entered_late_time)/1000) << "ms";
        was_behind=false;
    }
    if(was_behind && latency>last_printed_latency+latencyThresholdEnter){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing now late by " << (latency/1000) << "ms (threshold is " << (latencyThresholdEnter/1000) << "ms)";
        last_printed_latency+=latencyThresholdEnter;
    }
}

//======================================================================
void TriggerPrimitiveFinder::processing_thread(uint8_t first_register, uint8_t last_register)
{
    pthread_setname_np(pthread_self(), "processing");

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
    size_t nhits=0;
    // -------------------------------------------------------- 
    // Actually process
    int nmsg=0;
    bool first=true;

    m_readyForMessages.store(true);

    while(true){
        ProcessingTasks::ItemToProcess item;
        while(!m_itemsToProcess.read(item)){ std::this_thread::sleep_for(std::chrono::microseconds(10)); }
        ++nmsg;
        if(item.timestamp==ProcessingTasks::END_OF_MESSAGES) break;
        measure_latency(item);
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*item.scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        if(first){
            pi.setState(mcadc);
            dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(item.scs);
            m_fiber_no=frame->fiber_no();
            m_crate_no=frame->crate_no();
            m_slot_no=frame->slot_no();
            first=false;
        }
        pi.input=mcadc;
        // "Empty" the list of hits
        *primfind_dest=MAGIC;
        // Do the processing
        process_window_avx2(pi);
        // Create dune::TriggerPrimitives from the hits and put them in the queue for later retrieval
        nhits+=addHitsToQueue(item.timestamp, primfind_dest, m_triggerPrimitives);
        m_latestProcessedTimestamp.store(item.timestamp);
    }
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "Received " << nmsg << " messages. Found " << nhits << " hits";

    // -------------------------------------------------------- 
    // Cleanup
    delete[] primfind_dest;
}


/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
