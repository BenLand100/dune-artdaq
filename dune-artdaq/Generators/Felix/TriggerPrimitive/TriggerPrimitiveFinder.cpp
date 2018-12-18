#include "TriggerPrimitiveFinder.h"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "artdaq-core/Data/Fragment.hh"
#include "dune-raw-data/Overlays/FelixHitFragment.hh"

#include <cstddef> // For offsetof

using namespace dune;

TriggerPrimitiveFinder::TriggerPrimitiveFinder(uint32_t qsize, size_t timeWindowNumMessages, size_t nthreads)
    : m_primfind_tmp(new MessageCollectionADCs[nthreads*timeWindowNumMessages]),
      m_timeWindowNumMessages(timeWindowNumMessages),
      m_timeWindowNumFrames(timeWindowNumMessages*FRAMES_PER_MSG),
      m_messagesReceived(1), // Set to 1 not zero as a hack to not process a window on the first message received
      m_nthreads(nthreads),
      m_pcq(qsize),
      m_nWindowsProcessed(0),
      m_nPrimsFound(0),
      m_threadpool(nthreads+2),
//      m_threadpool2(nthreads),
      m_anyWindowsProcessedYet(false)
{
    DAQLogger::LogInfo("TriggerPrimitiveFinder::TriggerPrimitiveFinder") << "Starting TriggerPrimitiveFinder with " << m_nthreads << " threads";

    const uint8_t registers_per_thread=REGISTERS_PER_FRAME/m_nthreads;
    
    const uint8_t tap_exponent=6;
    const int multiplier=1<<tap_exponent; // 64
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two

    // TODO: ProcessingInfo points to but doesn't copy the taps it's
    // passed, so we make a long-lived copy here. We ought to not leak
    // this (and also do it properly)
    int16_t* taps_p=new int16_t[taps.size()];
    for(size_t i=0; i<taps.size(); ++i) taps_p[i]=taps[i];

    for(size_t i=0; i<m_nthreads; ++i){
        m_primfind_destinations.push_back(new uint16_t[m_timeWindowNumFrames*1000]);
      
        ProcessingInfo pi(m_primfind_tmp,
                          m_timeWindowNumFrames,
                          uint8_t(registers_per_thread*i),
                          uint8_t(registers_per_thread*(i+1)),
                          m_primfind_destinations[i],
                          taps_p, (uint8_t)taps.size(),
                          tap_exponent,
                          0);
      
        m_futures.push_back(std::move(boost::make_ready_future<ProcessingInfo>(pi).share()));
    } // end for(i=0 to nThreads)
    
}
  
TriggerPrimitiveFinder::~TriggerPrimitiveFinder()
{
    for(auto& f: m_futures){
        // DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "Waiting on " << &f;
        f.wait();
    }
    // Print some hits
    //
    // DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << "Destructing. Found " << m_nPrimsFound << " hits";
    // if(!m_windowHits.empty()){
    //     std::ostringstream os;
    //     for(size_t i=0; i<std::min(300ul, m_windowHits[0].triggerPrimitives.size()); ++i){
    //         dune::TriggerPrimitive& tp=m_windowHits[0].triggerPrimitives[i];
    //         os << tp.channel << " " << tp.startTimeOffset << " " << tp.charge << std::endl;
    //     }
    //     DAQLogger::LogInfo("TriggerPrimitiveFinder::~TriggerPrimitiveFinder") << os.str();
    // }
    delete[] m_primfind_tmp;
    for(auto& d: m_primfind_destinations) delete[] d;
}

void TriggerPrimitiveFinder::addMessage(SUPERCHUNK_CHAR_STRUCT& ucs)
{
    static bool first=true;
    static int nFullPrint=0;
    if(first){
        DAQLogger::LogInfo("TriggerPrimitiveFinder::addMessage") << "First call";
        first=false;
    }
    RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(ucs);
    MessageCollectionADCs* mca=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
    if(!m_pcq.write( std::move(*mca) )){
        ++nFullPrint;
        if(nFullPrint<10){
            DAQLogger::LogInfo("TriggerPrimitiveFinder::addMessage") << "Queue full";
        }
        else if(nFullPrint%1024==0){
            DAQLogger::LogInfo("TriggerPrimitiveFinder::addMessage") << "1024 more \"queue full\" messages suppressed";
        }
    }
    if((++m_messagesReceived)%m_timeWindowNumMessages==0){
        const dune::WIBHeader* wh = reinterpret_cast<const dune::WIBHeader*>(&ucs);
        process_window(wh->timestamp());
    }
}

void TriggerPrimitiveFinder::process_window(uint64_t timestamp)
{
    if(!m_anyWindowsProcessedYet.load()){
        DAQLogger::LogInfo("TriggerPrimitiveFinder::process_window") << "First call to process_window";
    }
    // We've got enough frames to reorder them. First, copy
    // the data into a working area for the thread.  Given the
    // way expand_message_adcs orders the data, this results
    // in the relevant segment of primfind_tmp containing:
    //
    // (register 0, time  0) (register 0, time  1) ... (register 0, time 11)
    // (register 1, time  0) (register 1, time  1) ... (register 1, time 11)
    // ...
    // (register 7, time  0) (register 7, time  1) ... (register 7, time 11)
    // 
    // (register 0, time 12) (register 0, time 13) ... (register 0, time 23)
    // (register 1, time 12) (register 1, time 13) ... (register 1, time 23)
    // ...
    // (register 7, time 12) (register 7, time 13) ... (register 7, time 23)
    // 
    // ...
    // ...                                         ... (register 7, time timeWindowNumFrames-1)


    // -----------------------------------------------------------------
    // Wait till all the currently running jobs are done...
    auto waiter=boost::when_all(m_futures.begin(), m_futures.end()).share();

    // boost::when_all spawns a new thread, which might be a
    // bottleneck(?). Write our own version using the executor to get
    // around that. NB: This has to go on a different executor to the
    // other futures, otherwise we get deadlock when there are more
    // outstanding tasks than threads in the pool

    // std::vector<boost::shared_future<ProcessingInfo>> tmp_futures=m_futures;
    // auto manual_waiter=boost::async(m_threadpool2, [tmp_futures](){
    //         for(auto& f: tmp_futures) f.wait();
    //         return tmp_futures;
    //     }).share();


    // -----------------------------------------------------------------
    // ...and then copy the data into the working area.
    //
    // We do different things depending on whether this is the first
    // time process_window() has been called: on the first call, we
    // have to "seed" the ProcessingInfo's with pedestal values etc;
    // on the subsequent calls, we take the previous ProcessingInfo
    // and pass it in
    typedef std::function<std::vector<ProcessingInfo>(decltype(waiter))> copy_func_t;

    // The function for the first call
    copy_func_t copy_lambda_first=[this](decltype(waiter) f){
        for(size_t j = 0; j < m_timeWindowNumMessages; j++){
            m_pcq.read(m_primfind_tmp[j]);
        }
        std::vector<ProcessingInfo> pis;
        DAQLogger::LogInfo("TriggerPrimitiveFinder::process_window") << "Processing first window";
        for(size_t i=0; i<m_nthreads; ++i){
            // make a copy of the ProcessingInfo which we'll update
            ProcessingInfo pi=f.get()[i].get();
            DAQLogger::LogInfo("TriggerPrimitiveFinder::process_window") << "Got ProcessingInfo with nhits=" << pi.nhits;
            pi.setState(m_primfind_tmp);
            pis.push_back(pi);
        }
        m_anyWindowsProcessedYet.store(true);
        
        return pis;
    };

    // The function for subsequent calls
    copy_func_t copy_lambda_nonfirst=[this, timestamp](decltype(waiter) f){
        addHitsToQueue(timestamp);
        // Copy the window into the temporary working area
        for(size_t j = 0; j < m_timeWindowNumMessages; j++){
            m_pcq.read(m_primfind_tmp[j]);
        }
        std::vector<ProcessingInfo> pis;

        for(size_t i=0; i<m_nthreads; ++i){
            ProcessingInfo pi=f.get()[i].get();
            pis.push_back(pi);
        }
        return pis;
    };
    // Set the appropriate function going on the thread pool
    auto copy_future=waiter.then(m_threadpool,
                                 m_anyWindowsProcessedYet.load() ?
                                 std::forward<copy_func_t>(copy_lambda_nonfirst) :
                                 std::forward<copy_func_t>(copy_lambda_first)).share();

    m_anyWindowsProcessedYet.store(true);
    // ...and *then* set all the jobs going again. Each of the
    // new futures waits on the copy task, which is what we want.
    for(size_t ithread=0; ithread<m_nthreads; ++ithread){
        m_futures[ithread]=copy_future.then(m_threadpool,
                                            [this, ithread](auto fin)
                                            {
                                                // MAGIC is the terminator for the list of hits, so this line "empties" the list
                                                *m_primfind_destinations[ithread]=MAGIC;
                                                return process_window_avx2(fin.get()[ithread]);
                                            });
    }

    ++m_nWindowsProcessed;
}

std::vector<dune::TriggerPrimitive>
TriggerPrimitiveFinder::primitivesForTimestamp(uint64_t timestamp, uint32_t window_size)
{
    std::vector<dune::TriggerPrimitive> ret;
    const int64_t signed_ts=timestamp;
    // TODO: m_windowHits might be accessed from another thread in
    // addHitsToQueue. Deal with this somehow
    for(auto const& wp: m_windowHits){
        if(std::abs(signed_ts-wp.timestamp)<window_size){
            ret.insert(ret.end(), wp.triggerPrimitives.begin(), wp.triggerPrimitives.end());
        }
    }
    return ret;
}

void TriggerPrimitiveFinder::addHitsToQueue(uint64_t timestamp)
{
    TriggerPrimitiveFinder::WindowPrimitives prims;
    prims.timestamp=timestamp;

    for(size_t j=0; j<m_nthreads; ++j){
        uint16_t chan[16], hit_start[16], hit_charge[16], hit_tover[16];
        uint16_t* input_loc=m_primfind_destinations[j];
        
        while(*input_loc!=MAGIC){
            for(int i=0; i<16; ++i) chan[i]       = collection_index_to_offline(*input_loc++);
            for(int i=0; i<16; ++i) hit_start[i]  = *input_loc++;
            for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
            for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;
            
            for(int i=0; i<16; ++i){
                if(hit_charge[i] && chan[i]!=MAGIC){
                    dune::TriggerPrimitive p{chan[i], hit_start[i], hit_charge[i], hit_tover[i]};
                    ++m_nPrimsFound;
                    prims.triggerPrimitives.push_back(std::move(p));
                }
            }
        }
    }
    m_windowHits.push_back(prims);
    if(m_windowHits.size()>1000) m_windowHits.pop_front();
}

void TriggerPrimitiveFinder::hitsToFragment(uint64_t timestamp, uint32_t window_size, artdaq::Fragment* fragPtr)
{
    std::vector<dune::TriggerPrimitive> tps=primitivesForTimestamp(timestamp, window_size);
    // The data payload of the fragment will be:
    // uint64_t timestamp
    // uint32_t nhits
    // N*TriggerPrimitive
    fragPtr->resizeBytes(sizeof(dune::FelixHitFragment::Body)+tps.size()*sizeof(TriggerPrimitive));
    dune::FelixHitFragment hitFrag(*fragPtr);
    hitFrag.set_timestamp(timestamp);
    hitFrag.set_nhits(tps.size());
    for(size_t i=0; i<tps.size(); ++i){
        hitFrag.get_primitive(i)=tps[i];
    }
}

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
