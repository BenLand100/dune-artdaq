#include "TriggerPrimitiveFinder.h"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixFragment.hh"

#include <cstddef> // For offsetof
#include <sstream>

#include <sys/time.h>
#include <sys/resource.h>

#include "numa.h"
#include "numaif.h"

#include "sched.h" // for sched_getcpu()

#include "tests/frames2array.h"

const int64_t clocksPerTPCTick=25;


//======================================================================
TriggerPrimitiveFinder::TriggerPrimitiveFinder(fhicl::ParameterSet const & ps)
//std::string zmq_hit_send_connection, uint32_t window_offset, int32_t cpu_offset, int item_queue_size)
    : m_readyForMessages(false),
      m_fiber_no(0xff),
      m_slot_no(0xff),
      m_crate_no(0xff),
      m_TPSender(std::make_unique<ptmp::TPSender>(ptmp_util::make_ptmp_socket_string("PUB", "bind", {ps.get<std::string>("zmq_hit_send_connection")}))),
      m_current_tpset(new ptmp::data::TPSet),
      m_send_ptmp_msgs(ps.get<bool>("send_ptmp_messages", true)),
      m_msgs_per_tpset(ps.get<unsigned int>("messages_per_tpset", 20)),
      m_should_stop(false),
      m_windowOffset(ps.get<uint32_t>("window_offset")),
      m_channels_to_suppress(ps.get<std::vector<uint32_t>>("channels_to_suppress", std::vector<uint32_t>())),
      m_offline_channel_base(0),
      m_n_tpsets_sent(0)
{
    std::vector<int32_t> cpus_to_pin=ps.get<std::vector<int32_t>>("cpus_to_pin", std::vector<int32_t>());
    size_t qsize=ps.get<size_t>("item_queue_size", 100000);
    m_processingThread=std::thread(&TriggerPrimitiveFinder::processing_thread, this, 0, REGISTERS_PER_FRAME, qsize, cpus_to_pin);
}

//======================================================================
TriggerPrimitiveFinder::~TriggerPrimitiveFinder()
{
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "TriggerPrimitiveFinder dtor entered";
    m_should_stop.store(false);
    // Superstition: add multiple "end of messages" messages
    for(int i=0; i<5; ++i){
        m_itemsToProcess->write(ProcessingTasks::ItemToProcess{ProcessingTasks::END_OF_MESSAGES, SUPERCHUNK_CHAR_STRUCT{}, ProcessingTasks::now_us()});
    }
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "Joining processing thread";
    m_processingThread.join(); // Wait for it to actually stop
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "Processing thread joined";
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "Sent a total of " << m_n_tpsets_sent << " TPSets";
}

//======================================================================
void TriggerPrimitiveFinder::stop()
{
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::stop") << "Setting stop flag";
    m_should_stop.store(true);
    // delete the TPSender so it closes its socket, hopefully before zsys_shutdown gets called
    m_TPSender.reset();
}

//======================================================================
bool TriggerPrimitiveFinder::addMessage(SUPERCHUNK_CHAR_STRUCT& ucs)
{
    static size_t nPrinted=0;
    if(m_readyForMessages.load()){
        // The first frame in the message
        dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(&ucs);
        const uint64_t timestamp=frame->timestamp();
        if(timestamp==0 && nPrinted<10){
            dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::addMessage") << "Got frame with timestamp zero!";
            frame->print();
            ++nPrinted;
        }
        return m_itemsToProcess->write(ProcessingTasks::ItemToProcess{timestamp, ucs, ProcessingTasks::now_us()});
    }
    return true;
}

//======================================================================
void TriggerPrimitiveFinder::hitsToFragment(uint64_t timestamp, uint32_t window_size, artdaq::Fragment* fragPtr)
{
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::hitsToFragment") << "Creating fragment for timestamp " << timestamp << " window_size " << window_size;
    std::vector<dune::TriggerPrimitive> tps=getHitsForWindow(m_triggerPrimitives,
                                                             timestamp-m_windowOffset*clocksPerTPCTick,
                                                             timestamp+window_size-m_windowOffset*clocksPerTPCTick);
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::hitsToFragment") << "Got " << tps.size() << " hits for timestamp 0x" << std::hex << timestamp << std::dec;

    // The data payload of the fragment will be:
    // uint64_t timestamp
    // uint32_t nhits
    // N*TriggerPrimitive
    fragPtr->resizeBytes(sizeof(dune::FelixFragmentHits::Body)+tps.size()*sizeof(dune::TriggerPrimitive));
    dune::FelixFragmentHits hitFrag(*fragPtr);

    hitFrag.set_timestamp(timestamp);
    hitFrag.set_nhits(tps.size());
    hitFrag.set_window_offset(m_windowOffset);

    hitFrag.set_fiber_no(m_fiber_no);
    hitFrag.set_slot_no(m_slot_no);
    hitFrag.set_crate_no(m_crate_no);

    for(size_t i=0; i<tps.size(); ++i){
        hitFrag.get_primitive(i)=tps[i];
    }

}

//======================================================================
std::vector<dune::TriggerPrimitive>
TriggerPrimitiveFinder::getHitsForWindow(const std::deque<dune::TriggerPrimitive>& primitive_queue,
                                         uint64_t start_ts, uint64_t end_ts)
{
    std::vector<dune::TriggerPrimitive> ret;
    auto startTime=std::chrono::steady_clock::now();
    auto now=std::chrono::steady_clock::now();
    // Wait for the processing to catch up, up to 1.5 second
    const size_t timeout_ms=1500;
    int nloops=0;
    while(m_latestProcessedTimestamp.load()<end_ts && (now - startTime) < std::chrono::milliseconds(timeout_ms)){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        now=std::chrono::steady_clock::now();
        ++nloops;
    }
    // We timed out
    if(m_latestProcessedTimestamp.load()<end_ts){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::getHitsForWindow") << "getHitsForWindow timed out waiting for timestamp " << end_ts << ". Latest processed timestamp is " << m_latestProcessedTimestamp.load() << ". waiting loop went " << nloops << " times";
        return ret;
    }

    ret.reserve(5000);

    {
        std::lock_guard<std::mutex> guard(m_triggerPrimitiveMutex);

        for(auto const& prim: primitive_queue){
            if(prim.messageTimestamp>start_ts && prim.messageTimestamp<end_ts){
                ret.push_back(prim);
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

    static unsigned int msgs_in_tpset=0;
    ptmp::data::TPSet& tpset=*m_current_tpset.get();
    
    if(m_send_ptmp_msgs && msgs_in_tpset==0){
        tpset.set_count(m_n_tpsets_sent);
        // m_*_no are uint8_t, so maybe the casts to uint32_t before shifting are necessary?
        tpset.set_detid((uint32_t(m_fiber_no) << 16) | (uint32_t(m_slot_no) << 8) | (uint32_t(m_crate_no) << 0));
        tpset.set_tstart(timestamp);
        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        // Brett asked for TPSet creation time in microseconds, not timing system ticks
        auto ticks = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch());
        tpset.set_created(ticks.count());
    }
    
    while(*input_loc!=MAGIC){
        for(int i=0; i<16; ++i) chan[i]       = *input_loc++;
        for(int i=0; i<16; ++i) hit_end[i]    = *input_loc++;
        for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
        for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;
        
        for(int i=0; i<16; ++i){
            if(hit_charge[i] && chan[i]!=MAGIC){
                const uint16_t online_channel=collection_index_to_channel(chan[i]);
                const uint32_t offline_channel=m_offline_channel_base+collection_index_to_offline(chan[i]);
                // Hack for now, to exclude high TP rate (>10kHz) channels. -JLS June 2019
                // if (offline_channel==9691 || offline_channel==5296 || offline_channel==5010 || offline_channel==4387
                // || offline_channel==4381 || offline_channel==4383 || offline_channel==5006 || offline_channel==9689) { continue; }
                primitive_queue.emplace_back(timestamp, online_channel, hit_end[i], hit_charge[i], hit_tover[i]);
                if(m_send_ptmp_msgs){
                    bool should_send_this=true;
                    for(auto const bad_ch: m_channels_to_suppress){
                        if(offline_channel==bad_ch) should_send_this=false;
                    }
                    if(should_send_this){
                        // hit_end is the end time of the hit in TPC clock
                        // ticks after the start of the netio message in which
                        // the hit ended
                        uint64_t hit_start=timestamp+clocksPerTPCTick*(int64_t(hit_end[i])-hit_tover[i]);
                        ptmp::data::TrigPrim* ptmp_prim=tpset.add_tps();
                        ptmp_prim->set_channel(offline_channel);
                        ptmp_prim->set_tstart(hit_start);
                        // Convert time-over-threshold to 50MHz clock ticks, so all the ptmp quantities are in the same units
                        ptmp_prim->set_tspan(clocksPerTPCTick*hit_tover[i]);
                        ptmp_prim->set_adcsum(hit_charge[i]);
                    }
                }
                ++nhits;
            }
        }
    }

    ++msgs_in_tpset;

    // Send out the TPSet every m_msgs_per_tpset messages
    if(m_send_ptmp_msgs && msgs_in_tpset==m_msgs_per_tpset){
        tpset.set_tspan(timestamp+FRAMES_PER_MSG*clocksPerTPCTick-tpset.tstart());
        // Don't send empty TPSets
        if(tpset.tps_size()!=0 && m_TPSender){
            ptmp::TPSender& tpsender=*m_TPSender;
            tpsender(tpset);
            ++m_n_tpsets_sent;
        }
        msgs_in_tpset=0;
        m_current_tpset.reset(new ptmp::data::TPSet);
    }

    while(primitive_queue.size()>20000) primitive_queue.pop_front();
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
    // The time in us between the timestamp on the data and now. In
    // principle, the full latency to get from the detector to this
    // hit-finding processing, on the assumption that the timing
    // system clock is in sync with this machine's clock
    int64_t full_latency=now-item.timestamp/50;
    // The time in us between the item being queued in addMessage()
    // and it starting processing. This is (sort of) the latency added
    // by TPF
    int64_t tpf_latency=now-item.timeQueued;

    static size_t nitem=0;
    if(nitem++ < 50){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Item with TS " << item.timestamp << " (ticks) queued at " << item.timeQueued << " (us), processed at " << now << " (us), full latency " << full_latency << "us, TPF latency " << tpf_latency << "us";
    }

    m_full_latency_hist.fill((uint64_t)std::max(0L, full_latency));
    m_tpf_latency_hist.fill((uint64_t)std::max(0L, tpf_latency));

    static int64_t last_printed_latency=0;
    if(!was_behind && full_latency > latencyThresholdEnter){
        entered_late_time=now;
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing late by " << (full_latency/1000) << "ms (threshold is " << (latencyThresholdEnter/1000) << "ms). Latency since queueing: " << (tpf_latency/1000) << "ms";
        was_behind=true;
        last_printed_latency=full_latency;
    }
    if(full_latency < latencyThresholdLeave && was_behind){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing caught up. Was late for " << ((now-entered_late_time)/1000) << "ms";
        was_behind=false;
    }
    if(was_behind && full_latency>last_printed_latency+latencyThresholdEnter){
        dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::measure_latency") << "Processing now late by " << (full_latency/1000) << "ms (threshold is " << (latencyThresholdEnter/1000) << "ms). Latency since queueing: " << tpf_latency << "us";
        last_printed_latency+=latencyThresholdEnter;
    }
}

//======================================================================
void TriggerPrimitiveFinder::processing_thread(uint8_t first_register, uint8_t last_register,
                                               size_t qsize, std::vector<int32_t> cpus_to_pin)
{
    pthread_setname_np(pthread_self(), "processing");

    if(!cpus_to_pin.empty()){
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        for(auto const& cpu: cpus_to_pin){
            CPU_SET(cpu, &cpuset);
        }
        pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    }
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "processing thread running on cpu " << sched_getcpu();

    // We allocate the queue of items in this thread *after* pinning
    // with the hope of making sure the allocation is on NUMA1 not
    // NUMA0, to spread the memory bandwidth load across NUMA cores
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "Allocating item queue of size " << qsize;
    m_itemsToProcess=std::make_unique<folly::ProducerConsumerQueue<ProcessingTasks::ItemToProcess>>(qsize);
    // numa(3) says: "All numa memory allocation policy only takes
    // effect when a page is actually faulted into the address space
    // of a process by accessing it". So write to all the slots in the
    // queue so the memory allocated definitely gets faulted in
    for(size_t i=0; i<qsize-1; ++i){
        m_itemsToProcess->write(ProcessingTasks::ItemToProcess{ProcessingTasks::END_OF_MESSAGES, SUPERCHUNK_CHAR_STRUCT{}, 0});
    }
    int node_counts[2]={0};
    for(size_t i=0; i<qsize-1; ++i){
        ProcessingTasks::ItemToProcess* item=m_itemsToProcess->frontPtr();
        node_counts[which_numa_node(item)]++;
        m_itemsToProcess->popFront();
    }
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "items on node 0: " << node_counts[0] << ", node 1: " << node_counts[1];
    uint64_t first_msg_us=0;

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
                      0,
                      0);

    // TODO: hardcoded paths :-O
    PdspChannelMapService channelMap("/nfs/sw/work_dirs/phrodrig/march2019-felix-trigger-primitive/srcs/dune_artdaq/dune-artdaq/Generators/Felix/TriggerPrimitive/protoDUNETPCChannelMap_RCE_v4.txt",
                                     "/nfs/sw/work_dirs/phrodrig/march2019-felix-trigger-primitive/srcs/dune_artdaq/dune-artdaq/Generators/Felix/TriggerPrimitive/protoDUNETPCChannelMap_FELIX_v4.txt");

    size_t nhits=0;
    // -------------------------------------------------------- 
    // Actually process
    int nmsg=0;
    bool first=true;

    m_readyForMessages.store(true);

    while(true){
        ProcessingTasks::ItemToProcess* item=nullptr;
        while(!(item=m_itemsToProcess->frontPtr()) && !m_should_stop.load()){ 
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        ++nmsg;
        if(first_msg_us==0) first_msg_us=ProcessingTasks::now_us();
        if(m_should_stop.load()){
            dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "m_should_stop is true. Exiting";
            break;
        }
        if(item->timestamp==ProcessingTasks::END_OF_MESSAGES){
            dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "Got END_OF_MESSAGES. Exiting";
            m_itemsToProcess->popFront();
            break;
        }
        measure_latency(*item);
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(item->scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        if(first){
            pi.setState(mcadc);
            dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(&item->scs);
            m_fiber_no=frame->fiber_no();
            m_crate_no=frame->crate_no();
            m_slot_no=frame->slot_no();
            // getOfflineChannel comes from frames2array.h. The magic
            // "48" is (maybe) the online channel number of the
            // lowest-numbered collection channel in the link. Found
            // by comparing the `offlines` and `index_to_chan` arrays
            // in frame_expand.h`: the [16] entry is 0 in offlines,
            // and 48 in `index_to_chan`
            m_offline_channel_base=getOfflineChannel(channelMap, frame, 48);

            first=false;
        }
        pi.input=mcadc;
        // "Empty" the list of hits
        *primfind_dest=MAGIC;
        // Do the processing
        process_window_avx2(pi);
        // Create dune::TriggerPrimitives from the hits and put them in the queue for later retrieval
        nhits+=addHitsToQueue(item->timestamp, primfind_dest, m_triggerPrimitives);
        m_latestProcessedTimestamp.store(item->timestamp);
        m_itemsToProcess->popFront();
    }
    uint64_t end_us=ProcessingTasks::now_us();
    int64_t walltime_us=end_us-first_msg_us;
    rusage r;
    getrusage(RUSAGE_THREAD, &r);
    double hitsPerMsg=double(nhits)/nmsg;
    double cputime_us=1e6*double(r.ru_utime.tv_sec)+double(r.ru_utime.tv_usec);
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::processing_thread") << "Received " << nmsg << " messages. Found " << nhits << " hits. hits/msg=" << hitsPerMsg << ". CPU time / wall time (from first message received) = " << (1e-6*cputime_us) << "s / " << (1e-6*walltime_us) << "s. Avg CPU % = " << (100*cputime_us/walltime_us);

    // Print the histograms of latencies
    print_latency_hist(m_full_latency_hist, "Full");
    print_latency_hist(m_tpf_latency_hist, "TPF");
    // -------------------------------------------------------- 
    // Cleanup
    delete[] primfind_dest;
}

void TriggerPrimitiveFinder::print_latency_hist(const PowerTwoHist<24>& hist, const std::string name) const
{
    std::stringstream latency_hist_ss;
    latency_hist_ss << name << " latency histogram (bin low edge, microseconds):" << std::endl;
    for(size_t i=0; i<hist.nbins(); ++i){
        latency_hist_ss << hist.binLo(i) << "\t" << hist.bin(i) << std::endl;
    }
    dune::DAQLogger::LogInfo("TriggerPrimitiveFinder::print_latency_hist") << latency_hist_ss.str();

}

int TriggerPrimitiveFinder::which_numa_node(void* p) const
{
    int get_mempolicy_node = -1;
    int ret=get_mempolicy(&get_mempolicy_node, NULL, 0, p, MPOL_F_NODE | MPOL_F_ADDR);
    if(ret==-1) return -1;
    return get_mempolicy_node;
}

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
