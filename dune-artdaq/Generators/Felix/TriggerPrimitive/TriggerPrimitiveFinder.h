#ifndef TRIGGERPRIMITIVEFINDER_H
#define TRIGGERPRIMITIVEFINDER_H

#include "frame_expand.h"
#include "process_avx2.h"
#include "design_fir.h"
#include "PdspChannelMapService.h"

#include "dune-artdaq/Generators/Felix/FelixFormat.hh"
#include "dune-artdaq/Generators/Felix/Types.hh"
#include "dune-raw-data/Overlays/FelixHitFormat.hh"

#include "boost/thread/future.hpp"
#include "boost/thread/executors/basic_thread_pool.hpp"

#include <deque>

namespace artdaq
{
    class Fragment;
}

class TriggerPrimitiveFinder
{
public:
    // struct TriggerPrimitive
    // {
    //     uint16_t channel;
    //     uint16_t startTimeOffset; // relative to 64-bit timestamp
    //     uint16_t charge;
    //     uint16_t timeOverThreshold;
    // };

    struct WindowPrimitives
    {
        uint64_t timestamp;
        std::vector<dune::TriggerPrimitive> triggerPrimitives;
    };

    TriggerPrimitiveFinder(uint32_t qsize, size_t timeWindowNumMessages, size_t nthreads);
  
    ~TriggerPrimitiveFinder();

    void addMessage(SUPERCHUNK_CHAR_STRUCT& ucs);

    void process_window(uint64_t timestamp);
  
    std::vector<dune::TriggerPrimitive> primitivesForTimestamp(uint64_t timestamp, uint32_t window_size);

    size_t getNMessages() const { return m_messagesReceived; }
    size_t getNWindowsProcessed() const { return m_nWindowsProcessed; }
    size_t getNPrimitivesFound() const { return m_nPrimsFound; }

    // Find all the hits around `timestamp` and write them into the fragment at fragPtr
    void hitsToFragment(uint64_t timestamp, uint32_t windowSize, artdaq::Fragment* fragPtr);

    void waitForJobs() { for(auto& f: m_futures) f.wait(); }
private:

    // Update a maximum counter atomically
    // From https://stackoverflow.com/questions/16190078
    void update_maximum(std::atomic<uint64_t>& maximum_value, uint64_t const& value) noexcept
    {
        uint64_t prev_value = maximum_value;
        while(prev_value < value &&
              !maximum_value.compare_exchange_weak(prev_value, value))
            ;
    }

    uint16_t getOfflineChannel(SUPERCHUNK_CHAR_STRUCT& ucs, unsigned int ch);
    void addHitsToQueue(uint64_t timestamp);

    std::vector<uint16_t*> m_primfind_destinations;
    std::vector<boost::shared_future<ProcessingInfo>> m_futures;
    MessageCollectionADCs* m_primfind_tmp;
    size_t m_timeWindowNumMessages;
    size_t m_timeWindowNumFrames;
    size_t m_messagesReceived;
    size_t m_nthreads;
    folly::ProducerConsumerQueue<MessageCollectionADCs> m_pcq;
    std::deque<WindowPrimitives> m_windowHits;
    size_t m_nWindowsProcessed;
    std::atomic<size_t> m_nPrimsFound;
    boost::basic_thread_pool m_threadpool;
    // boost::basic_thread_pool m_threadpool2;
    std::atomic<bool> m_anyWindowsProcessedYet;
    std::atomic<uint64_t> m_latestProcessedTimestamp;
    PdspChannelMapService m_channelMapService;
    uint16_t m_offlineChannelOffset;
};

#endif

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
