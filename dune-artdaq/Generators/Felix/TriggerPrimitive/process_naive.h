#ifndef PROCESS_NAIVE_H
#define PROCESS_NAIVE_H

#include "frame_expand.h"
#include "constants.h"
#include "ProcessingInfo.h"

void frugal_accum_update(int16_t& m, const int16_t s, int16_t& acc, const int16_t acclimit)
{
    if(s>m) ++acc;
    if(s<m) --acc;

    if(acc>=acclimit){
        ++m;
        acc=0;
    }

    if(acc<=-1*acclimit){
        --m;
        acc=0;
    }
}


ProcessingInfo
process_window_naive(ProcessingInfo info_in)
{
    ProcessingInfo info=info_in;

    // Start with taps as floats that add to 1. Multiply by some
    // power of two (2**N) and round to int. Before filtering, cap the
    // value of the input to INT16_MAX/(2**N)
    const size_t NTAPS=8;
    const int16_t adcMax=info.adcMax;

    uint16_t* output_loc=info.output;
    uint16_t* input16=(uint16_t*)info.input;
    int nhits=0;

    for(size_t ichan=0; ichan<REGISTERS_PER_FRAME*SAMPLES_PER_REGISTER; ++ichan){
        const size_t register_index=ichan/SAMPLES_PER_REGISTER;
        if(register_index<info.first_register || register_index>=info.last_register) continue;
        const size_t register_offset=ichan%SAMPLES_PER_REGISTER;

        const size_t register_t0_start=register_index*SAMPLES_PER_REGISTER*FRAMES_PER_MSG;

        // Get all the state variables by reference so they "automatically" get saved for the next go-round
        ChanState& state=info.chanState;
        int16_t& median=state.pedestals[ichan];
        int16_t& quantile25=state.quantile25[ichan];
        int16_t& quantile75=state.quantile75[ichan];
        int16_t& accum=state.accum[ichan];
        int16_t& accum25=state.accum25[ichan];
        int16_t& accum75=state.accum75[ichan];

        // Variables for filtering
        int16_t* prev_samp=state.prev_samp+NTAPS*ichan;

        // Variables for hit finding
        int16_t& prev_was_over=state.prev_was_over[ichan]; // was the previous sample over threshold?
        int16_t& hit_start=state.hit_start[ichan]; // start time of the hit
        int16_t& hit_charge=state.hit_charge[ichan];
        int16_t& hit_tover=state.hit_tover[ichan]; // time over threshold

        for(size_t itime=0; itime<info.timeWindowNumFrames; ++itime){
            const size_t msg_index=itime/12;
            const size_t msg_time_offset=itime%12;
            // The index in uint16_t of the start of the message we want
            const size_t msg_start_index=msg_index*sizeof(MessageCollectionADCs)/sizeof(uint16_t);
            const size_t offset_within_msg=register_t0_start+SAMPLES_PER_REGISTER*msg_time_offset+register_offset;
            const size_t index=msg_start_index+offset_within_msg;

            // --------------------------------------------------------------
            // Pedestal finding/coherent noise removal
            // --------------------------------------------------------------
            int16_t sample=input16[index];

            if(sample<median) frugal_accum_update(quantile25, sample, accum25, 10);
            if(sample>median) frugal_accum_update(quantile75, sample, accum75, 10);
            frugal_accum_update(median, sample, accum, 10);

            const int16_t sigma=quantile75-quantile25;
            if(ichan==0 && itime<100) printf("loq: %d upq: %d iqr: %d\n", quantile25, quantile75, sigma);

            sample-=median;

            // if(STORE_INTERMEDIATE) data->pedsub[index]=sample;

            // --------------------------------------------------------------
            // Filtering
            // --------------------------------------------------------------

            // Don't let the sample exceed adcMax, which is the value
            // at which its filtered version might overflow
            sample=std::min(sample, adcMax);
            int16_t filt_tmp=0;
            for(size_t j=0; j<NTAPS; ++j){
                filt_tmp+=info.taps[j]*prev_samp[(j+itime)%NTAPS];
            }
            prev_samp[itime%NTAPS]=sample;
            int16_t filt=filt_tmp;

            // if(ichan==0) std::cout << filt << " ";
            // if(STORE_INTERMEDIATE) data->filtered[index]=filt;

            // --------------------------------------------------------------
            // Hit finding
            // --------------------------------------------------------------
            bool is_over=filt > 5*sigma*info.multiplier;
            // if(ichan==0) std::cerr << itime << std::endl;
            if(is_over && !prev_was_over){
                // if(ichan==0) std::cerr << "\t" << itime << std::endl;
                hit_start=itime;
            }
            if(is_over){
                hit_charge+=filt;
                hit_tover++;
                prev_was_over=true;
            }
            if(prev_was_over && !is_over){
                // We reached the end of the hit: write it out
                (*output_loc++) = (uint16_t)ichan;
                (*output_loc++) = hit_start;
                (*output_loc++) = hit_charge;
                (*output_loc++) = hit_tover;

                hit_start=0;
                hit_charge=0;
                hit_tover=0;

                ++nhits;
                prev_was_over=false;
            } // end if left hit

        } // end loop over samples
    } // end loop over channels
    // printf("Found %d hits\n", nhits);
    // Write a magic "end-of-hits" value into the list of hits
    for(int i=0; i<4; ++i) (*output_loc++) = MAGIC;
    return info;
}
#endif
