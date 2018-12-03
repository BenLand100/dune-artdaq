#include "FrameFile.h"
#include "dune-artdaq/Generators/Felix/FelixFormat.hh"

#include "../frame_expand.h"
#include "../process_naive.h"
#include "../process_avx2.h"
#include "save_hits.h"
#include "../design_fir.h"
#include "../ProcessingInfo.h"

int main(int, char**)
{
    const size_t timeWindowNumMessages=FrameFile::frames_per_fragment/FRAMES_PER_MSG;
    const size_t timeWindowNumFrames=FRAMES_PER_MSG*timeWindowNumMessages;
    FrameFile ff("/data/lar/dunedaq/rodrigues/protodune-noise/felixcosmics.dat");
    FelixFrame* frame=ff.fragment(0);
    const size_t nmessages=FrameFile::frames_per_fragment/FRAMES_PER_MSG;
    // TODO: Come up with a sensible size for the output array
    size_t outputsize=100*96*FrameFile::frames_per_fragment;
    std::vector<uint16_t> output_naive(outputsize, 0);
    std::vector<uint16_t> output_avx2(outputsize, 0);
    std::vector<MessageCollectionADCs> mcas(timeWindowNumMessages);
    
    const int multiplier=1<<6;
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two

    for(size_t imsg=0; imsg<nmessages; ++imsg){
        USR_CHAR_STRUCT* ucs=reinterpret_cast<USR_CHAR_STRUCT*>(frame+imsg*FRAMES_PER_MSG);
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*ucs);
        MessageCollectionADCs* mca=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        mcas[imsg%timeWindowNumMessages]=*mca;
        if(imsg>0 && (imsg%timeWindowNumMessages==timeWindowNumMessages-1)){
            std::cout << "Processing window " << (imsg/timeWindowNumMessages) << std::endl;
            memset(output_naive.data(), 0, outputsize*sizeof(decltype(output_naive)::value_type));
            memset(output_avx2.data(), 0, outputsize*sizeof(decltype(output_avx2)::value_type));
            ProcessingInfo pi(mcas.data(),
                        timeWindowNumFrames,
                        0,
                        REGISTERS_PER_FRAME,
                        output_naive.data(),
                        taps.data(), taps.size(),
                        multiplier,
                              0);
            pi.setState(mcas.data());

            ProcessingInfo pi_avx2(mcas.data(),
                        timeWindowNumFrames,
                        0,
                        REGISTERS_PER_FRAME,
                        output_avx2.data(),
                        taps.data(), taps.size(),
                        multiplier,
                              0);
            pi_avx2.setState(mcas.data());

            process_window_naive(std::move(pi));
            process_window_avx2(std::move(pi_avx2));
        }
    }
    save_naive_hits(output_naive.data(), "naive-hits.txt");
    save_avx2_hits(output_avx2.data(), "avx2-hits.txt");
}
