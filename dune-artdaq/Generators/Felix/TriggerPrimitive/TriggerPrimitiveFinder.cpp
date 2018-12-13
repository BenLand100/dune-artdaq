#include "TriggerPrimitiveFinder.h"

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "artdaq-core/Data/Fragment.hh"

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
      m_anyWindowsProcessedYet(false)
{
    DAQLogger::LogInfo("TriggerPrimitiveFinder::TriggerPrimitiveFinder") << "Starting TriggerPrimitiveFinder with " << m_nthreads << " threads";

    const uint8_t registers_per_thread=REGISTERS_PER_FRAME/m_nthreads;
    
    const int multiplier=1<<6;
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two
    
    for(size_t i=0; i<m_nthreads; ++i){
        m_primfind_destinations.push_back(new uint16_t[m_timeWindowNumFrames*100]);
      
        ProcessingInfo pi(m_primfind_tmp,
                          m_timeWindowNumFrames,
                          uint8_t(registers_per_thread*i),
                          uint8_t(registers_per_thread*(i+1)),
                          m_primfind_destinations[i],
                          taps.data(), (uint8_t)taps.size(),
                          multiplier,
                          0);
      
        m_futures.push_back(std::move(boost::make_ready_future<ProcessingInfo>(pi).share()));
    } // end for(i=0 to nThreads)
    
}
  
TriggerPrimitiveFinder::~TriggerPrimitiveFinder()
{
    for(auto& f: m_futures) f.wait();
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
        process_window();
    }
}

void TriggerPrimitiveFinder::process_window()
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
    // Wait till all the currently running jobs are done...

    // auto waiter=boost::when_all(m_futures.begin(), m_futures.end()).share();

    // boost::when_all spawns a new thread, which might be a
    // bottleneck(?). Write our own version using the executor to get
    // around that
    std::vector<boost::shared_future<ProcessingInfo>> tmp_futures=m_futures;
    auto manual_waiter=boost::async(m_threadpool, [tmp_futures](){
            for(auto& f: tmp_futures) f.wait();
            return tmp_futures;
        }).share();

    // ...and then copy the data into the working area
    auto copy_future=manual_waiter.then(m_threadpool,
                                 [&](decltype(manual_waiter) f){
                                     // Copy out the hits from the last go-round
                                     // TODO: Real timestamp
                                     if(m_anyWindowsProcessedYet.load()) addHitsToQueue(0);
                                     // Copy the window into the temporary working area
                                     for(size_t j = 0; j < m_timeWindowNumMessages; j++){
                                         m_pcq.read(m_primfind_tmp[j]);
                                     }
                                     std::vector<ProcessingInfo> pis;
                                     // If this is the first message we've processed,
                                     // set the pedestals, otherwise just copy
                                     // the ProcessingInfo unchanged from the input
                                     if(!m_anyWindowsProcessedYet.load()){
                                         DAQLogger::LogInfo("TriggerPrimitiveFinder::process_window") << "Processing first window";
                                         // WindowCollectionADCs window(opt.timeWindowNumMessages, primfind_tmp);
                                         for(size_t i=0; i<m_nthreads; ++i){
                                             // make a copy of the ProcessingInfo which we'll update
                                             ProcessingInfo pi=f.get()[i].get();
                                             pi.setState(m_primfind_tmp);
                                             pis.push_back(pi);
                                         }
                                         m_anyWindowsProcessedYet.store(true);
                                     }
                                     else{
                                         for(size_t i=0; i<m_nthreads; ++i){
                                             pis.push_back(f.get()[i].get());
                                         }
                                     }

                                     return pis;
                                 }).share();
    // ...and *then* set all the jobs going again. Each of the
    // new futures waits on the copy task, which is what we want.
    //
    // TODO: We're replacing the items in the vector of
    // futures. What happens to the old ones? Does the futures
    // system keep hold of them somehow?
    for(size_t ithread=0; ithread<m_nthreads; ++ithread){
        m_futures[ithread]=copy_future.then(m_threadpool,
                                            [ithread](auto fin)
                                            {
                                                return process_window_avx2(fin.get()[ithread]);
                                            });
    }

    ++m_nWindowsProcessed;
}

std::vector<TriggerPrimitiveFinder::TriggerPrimitive>
TriggerPrimitiveFinder::primitivesForTimestamp(uint64_t timestamp, uint32_t window_size)
{
    std::vector<TriggerPrimitiveFinder::TriggerPrimitive> ret;
    const int64_t signed_ts=timestamp;
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
                    TriggerPrimitiveFinder::TriggerPrimitive p{chan[i], hit_start[i], hit_charge[i], hit_tover[i]};
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
    std::vector<TriggerPrimitive> tps=primitivesForTimestamp(timestamp, window_size);
    // The data payload of the fragment will be:
    // uint64_t timestamp
    // uint32_t nhits
    // N*TriggerPrimitive
    fragPtr->resizeBytes(sizeof(uint64_t)+sizeof(uint32_t)+tps.size()*sizeof(TriggerPrimitive));
    // TODO: Put this logic into the FelixHitFragment somehow so this code can be less terrifying
    uint8_t* bytes = fragPtr->dataBeginBytes();
    *(reinterpret_cast<uint64_t*>(bytes)) = timestamp;
    *(reinterpret_cast<uint32_t*>(bytes+sizeof(uint64_t))) = tps.size();
    TriggerPrimitive* tps_out=reinterpret_cast<TriggerPrimitive*>(bytes+sizeof(uint64_t)+sizeof(uint32_t));
    for(size_t i=0; i<tps.size(); ++i){
        tps_out[i]=tps[i];
    }
}

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
