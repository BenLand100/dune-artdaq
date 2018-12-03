#ifndef PROCESSINGINFO_H
#define PROCESSINGINFO_H

#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include <boost/thread/future.hpp>

#include "frame_expand.h"

// The state variables for each channel in the link, saved from the last time
struct ChanState
{
    ChanState()
    {
        for(size_t i=0; i<REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER; ++i){
            pedestals[i]=0;
            accum[i]=0;
            accum25[i]=0;
            accum75[i]=0;
            prev_was_over[i]=0;
            hit_start[i]=0;
            hit_charge[i]=0;
            hit_tover[i]=0;
            for(size_t j=0; j<NTAPS; ++j) prev_samp[i*NTAPS+j]=0;
        }
    }

    // TODO: DRY
    static const int NTAPS=8;

    int16_t pedestals[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];
    int16_t quantile25[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];
    int16_t quantile75[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];

    int16_t accum[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];
    int16_t accum25[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];
    int16_t accum75[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];

    // Variables for filtering
    int16_t prev_samp[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER*NTAPS];

    // Variables for hit finding
    int16_t prev_was_over[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER]; // was the previous sample over threshold?
    int16_t hit_start[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER]; // start time of the hit
    int16_t hit_charge[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER];
    int16_t hit_tover[REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER]; // time over threshold
};

struct ProcessingInfo
{
    ProcessingInfo(const MessageCollectionADCs* __restrict__ input_,
                   size_t timeWindowNumFrames_,
                   uint8_t first_register_,
                   uint8_t last_register_,
                   uint16_t* __restrict__ output_,
                   const int16_t* __restrict__ taps_,
                   int16_t /* ntaps_ */,
                   const int16_t multiplier_,
                   size_t nhits_)
        : input(input_),
          timeWindowNumFrames(timeWindowNumFrames_),
          first_register(first_register_),
          last_register(last_register_),
          output(output_),
          taps(taps_),
          multiplier(multiplier_),
          adcMax(INT16_MAX/multiplier_),
          nhits(nhits_)
    {
    }

    // Set the initial state from the window starting at first_msg_p
    void setState(MessageCollectionADCs* first_msg_p)
    {
        // Set the pedestals and the 25/75-percentiles: everything else is just the same as in default values
        WindowCollectionADCs window(timeWindowNumFrames/FRAMES_PER_MSG, first_msg_p);
        for(size_t j=0; j<REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER; ++j){
            const int16_t ped=window.get16(j, 0);
            chanState.pedestals[j]=ped;
            chanState.quantile25[j]=ped-3;
            chanState.quantile75[j]=ped+3;
        }
    }

    const MessageCollectionADCs* __restrict__ input;
    size_t timeWindowNumFrames;
    uint8_t first_register;
    uint8_t last_register;
    uint16_t* __restrict__ output;
    const int16_t* __restrict__ taps;
    int16_t ntaps;
    int16_t multiplier;
    int16_t adcMax;
    size_t nhits;
    ChanState chanState;
};


#endif
