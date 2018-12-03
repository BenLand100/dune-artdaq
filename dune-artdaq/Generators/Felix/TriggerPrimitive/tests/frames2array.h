#ifndef FRAMES2ARRAY_H
#define FRAMES2ARRAY_H

#include "../frame_expand.h"

#include "dune-artdaq/Generators/Felix/FelixFormat.hh"
#include "../PdspChannelMapService.h"
#include <iostream>
#include <vector>
#include <set>
//======================================================================
unsigned int getOfflineChannel(PdspChannelMapService& channelMap, const FelixFrame* frame, unsigned int ch)
{
    // handle 256 channels on two fibers -- use the channel
    // map that assumes 128 chans per fiber (=FEMB) (Copied
    // from PDSPTPCRawDecoder_module.cc)
    int crate = frame->crate_no();
    int slot = frame->slot_no();
    int fiber = frame->fiber_no();

    unsigned int fiberloc = 0;
    if (fiber == 1){
        fiberloc = 1;
    }
    else if(fiber == 2){
        fiberloc = 3;
    }
    else{
        std::cout << " Fiber number " << (int) fiber << " is expected to be 1 or 2 -- revisit logic" << std::endl;
        fiberloc = 1;
    }

    unsigned int chloc = ch;
    if (chloc > 127){
        chloc -= 128;
        fiberloc++;
    }
    unsigned int crateloc = crate;
    return channelMap.GetOfflineNumberFromDetectorElements(crateloc, slot, fiberloc, chloc, PdspChannelMapService::kFELIX);
}

//======================================================================
size_t calc_ntimes(const FelixFrame* frames, size_t nframes)
{
    uint64_t prev_ts=0;
    size_t ntimes=0;
    for(size_t iframe=0; iframe<nframes; ++iframe){
        const FelixFrame* frame=frames+iframe;
        uint64_t timestamp = frame->timestamp();
        if(timestamp<prev_ts){
            ntimes=iframe;
            break;
        }
        prev_ts=timestamp;
    }
    std::cout << "There are " << ntimes << " times" << std::endl;
    return ntimes;
}

//======================================================================
void fragment_frames_to_array(const FelixFrame* frames, size_t nframes,
                              uint16_t* array, std::vector<uint16_t>& channel_vec)
{
    PdspChannelMapService channelMap("../data/protoDUNETPCChannelMap_RCE_v4.txt", "../data/protoDUNETPCChannelMap_FELIX_v4.txt");

    // Milo's file is ordered with all the frames from one link, in
    // time order, followed by all the frames from the next link in
    // time order, and so on
    uint64_t prev_ts=0;
    channel_vec.resize(FelixFrame::num_ch_per_frame, 0);

    std::set<uint64_t> alltimes;
    for(size_t iframe=0; iframe<nframes; ++iframe){
        const FelixFrame* frame=frames+iframe;
        int64_t timestamp = frame->timestamp();
        // std::cout << timestamp << std::endl;
        if(prev_ts!=0 && (timestamp-prev_ts!=25)){
            std::cout << "ts=" << timestamp << " prev_ts=" << prev_ts << " delta=" << (timestamp-prev_ts) << std::endl;
        }
        alltimes.insert(timestamp);
        for(unsigned ch=0; ch<FelixFrame::num_ch_per_frame; ++ch){
            unsigned int offlineChannel=getOfflineChannel(channelMap, frame, ch);
            uint16_t oldOffline=channel_vec.at(ch);
            if(oldOffline!=0 && (oldOffline!=offlineChannel)){
                std::cout << "Channel changed. Was " << oldOffline << " now " << offlineChannel << std::endl;
            }
            channel_vec.at(ch)=offlineChannel;
            // Arrange the output so all of a given channel's times are in order
            array[FelixFrame::num_ch_per_frame*iframe+ch]=frame->channel(ch);
        }
        // std::cout << "ts=" << (timestamp-prev_ts) << " crate=" << crate << " slot=" << slot << " fiber=" << fiber << " offlineChannel=" << offlineChannel << std::endl;
        prev_ts=timestamp;
    }
    std::cout << "Saw " << alltimes.size() << " distinct timestamps" << std::endl;
}

//======================================================================
void fragment_frames_to_array_collection(const FelixFrame* frames, size_t nframes, uint16_t* array)
{
    PdspChannelMapService channelMap("../data/protoDUNETPCChannelMap_RCE_v4.txt", "../data/protoDUNETPCChannelMap_FELIX_v4.txt");
    
    size_t counter=0;
    for(size_t iframe=0; iframe<nframes; ++iframe){
        const FelixFrame* frame=frames+iframe;
        RegisterArray<8> frame_colls=get_frame_collection_adcs(frame);
        for(int ich=0; ich<8; ++ich){
            _mm256_storeu_si256(((__m256i*)array)+counter, frame_colls.ymm(ich));
            ++counter;
        }
    }
}

#endif
