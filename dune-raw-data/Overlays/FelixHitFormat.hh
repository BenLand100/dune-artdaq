#ifndef artdaq_dune_Overlays_FelixHitFormat_hh
#define artdaq_dune_Overlays_FelixHitFormat_hh

namespace dune
{
    struct TriggerPrimitive
    {
        TriggerPrimitive(uint64_t messageTimestamp_,
                         uint16_t channel_,
                         uint16_t endTime_,
                         uint16_t charge_,
                         uint16_t timeOverThreshold_)
            : messageTimestamp(messageTimestamp_),
              channel(channel_), 
              endTime(endTime_), 
              charge(charge_), 
              timeOverThreshold(timeOverThreshold_)
        {}

        uint64_t messageTimestamp;   // The timestamp of the netio message that this hit comes from
        uint16_t channel;            // The electronics channel number within the (crate, slot, fiber)
        uint16_t endTime;            // In TPC ticks relative to the start of the netio message
        uint16_t charge;             // In ADC
        uint16_t timeOverThreshold;  // In *TPC* clock ticks
    };
}

#endif // include guard
