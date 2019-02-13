#ifndef TRIGGERPRIMITIVEFINDER_H
#define TRIGGERPRIMITIVEFINDER_H

#include "frame_expand.h"
#include "process_avx2.h"
#include "design_fir.h"
#include "ProcessingTasks.h"

// #include "dune-artdaq/Generators/Felix/FelixFormat.hh"
#include "dune-artdaq/Generators/Felix/Types.hh"
#include "dune-raw-data/Overlays/FelixHitFormat.hh"

#include <deque>
#include <mutex>

#include "zmq.h"

namespace artdaq
{
    class Fragment;
}

class TriggerPrimitiveFinder
{
public:
    TriggerPrimitiveFinder(int32_t cpu_offset=-1);
  
    ~TriggerPrimitiveFinder();

    void addMessage(SUPERCHUNK_CHAR_STRUCT& ucs);

    // Find all the hits around `timestamp` and write them into the fragment at fragPtr
    void hitsToFragment(uint64_t timestamp, uint32_t windowSize, artdaq::Fragment* fragPtr);

    std::vector<dune::TriggerPrimitive> getHitsForWindow(uint64_t start_ts, uint64_t end_ts)
    {
        return getHitsForWindow(m_triggerPrimitives, start_ts, end_ts);
    }

    // void waitForJobs() { for(auto& f: m_futures) f.wait(); }
private:

    void processing_thread(void* context, uint8_t first_register, uint8_t last_register);

    std::vector<dune::TriggerPrimitive> getHitsForWindow(const std::deque<dune::TriggerPrimitive>& primitive_queue,
                                                         uint64_t start_ts, uint64_t end_ts);

    // Update a maximum counter atomically
    // From https://stackoverflow.com/questions/16190078
    void update_maximum(std::atomic<uint64_t>& maximum_value, uint64_t const& value) noexcept
    {
        uint64_t prev_value = maximum_value;
        while(prev_value < value &&
              !maximum_value.compare_exchange_weak(prev_value, value))
            ;
    }

    unsigned int addHitsToQueue(uint64_t timestamp,
                                const uint16_t* input_loc,
                                std::deque<dune::TriggerPrimitive>& primitive_queue);


    void measure_latency(const ItemToProcess& item);

    // The queue of trigger primitives found, and a mutex to protect it
    std::deque<dune::TriggerPrimitive> m_triggerPrimitives;
    std::mutex m_triggerPrimitiveMutex;

    std::atomic<uint64_t> m_latestProcessedTimestamp;
    void* m_zmq_context;
    std::thread m_processingThread;
    ItemPublisher m_itemPublisher;
};

#endif

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
