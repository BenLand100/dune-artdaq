#ifndef TRIGGERPRIMITIVEFINDER_H
#define TRIGGERPRIMITIVEFINDER_H

#include "frame_expand.h"
#include "process_avx2.h"
#include "design_fir.h"
#include "ProcessingTasks.h"

// #include "dune-artdaq/Generators/Felix/FelixFormat.hh"
#include "dune-artdaq/Generators/Felix/Types.hh"
#include "dune-raw-data/Overlays/FelixHitFormat.hh"

#include "fhiclcpp/ParameterSet.h"

#include "ptmp/api.h"

#include <deque>
#include <mutex>

#include "zmq.h"

#include "PdspChannelMapService.h"

namespace artdaq
{
    class Fragment;
}

class TriggerPrimitiveFinder
{
public:
    //TriggerPrimitiveFinder(std::string zmq_hit_send_connection, uint32_t window_offset, int32_t cpu_offset=-1, int item_queue_size=100000);
    TriggerPrimitiveFinder(fhicl::ParameterSet const & ps);
  
    ~TriggerPrimitiveFinder();

    // Returns true if message was successfully added, false if message queue was full because we're too far behind
    bool addMessage(SUPERCHUNK_CHAR_STRUCT& ucs);

    // Find all the hits around `timestamp` and write them into the fragment at fragPtr
    void hitsToFragment(uint64_t timestamp, uint32_t windowSize, artdaq::Fragment* fragPtr);

    std::vector<dune::TriggerPrimitive> getHitsForWindow(uint64_t start_ts, uint64_t end_ts)
    {
        return getHitsForWindow(m_triggerPrimitives, start_ts, end_ts);
    }

    void stop();

    // Are we ready to receive data? (ie, has the processing thread successfully started up?)
    bool readyForMessages() const { return m_readyForMessages.load(); }
private:

    void processing_thread(uint8_t first_register, uint8_t last_register);

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


    void measure_latency(const ProcessingTasks::ItemToProcess& item);


    // The queue of trigger primitives found, and a mutex to protect it
    std::deque<dune::TriggerPrimitive> m_triggerPrimitives;
    std::mutex m_triggerPrimitiveMutex;

    std::atomic<uint64_t> m_latestProcessedTimestamp;
    std::thread m_processingThread;
    std::atomic<bool> m_readyForMessages;
    folly::ProducerConsumerQueue<ProcessingTasks::ItemToProcess> m_itemsToProcess;
    // The electronics co-ordinates of the link we're getting data
    // from. We assume that this class will only deal with data from
    // one link
    uint8_t m_fiber_no;
    uint8_t m_slot_no;
    uint8_t m_crate_no;
    ptmp::TPSender m_TPSender;
    std::unique_ptr<ptmp::data::TPSet> m_current_tpset;
    bool m_send_ptmp_msgs;
    unsigned int m_msgs_per_tpset;
    std::atomic<bool> m_should_stop;
    uint32_t m_windowOffset;
    uint32_t m_offline_channel_base;
    size_t m_n_tpsets_sent;
};

#endif

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
