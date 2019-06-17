#include "frames2array.h"
#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "FrameFile.h"

#include <fstream>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <set>

int main(int, char**)
{
    // Quoth Milo (stitched together from a Slack DM on  2018-11-21):
    //
    // The file contains the 50 first fragments in run 5863. A
    // fragment contains a readout window for a given fibre. In the
    // run that this data is from, that means it contains 15024 frames
    // coming from that same fibre, corresponding to a readout window
    // of 7.5 ms. [The file] contains the same data volume as 5 APA
    // readouts. The fragments are not strictly ordered within a root
    // file, if I remember correctly, so you might have 6 fragments
    // for one fibre and 4 for another. I just took 50 fragments
    // because that way i could be quite sure that all fibres are
    // represented in the data. The first 15024 are from one fibre and
    // are in chronological order. Every timestamp within the fragment
    // data should be 25 more than the one in the previous frame. If
    // you notice any deviation in this or the identifier fields
    // (slot, crate, fiber numbers), you know that the data is
    // corrupt.  The same should be the case for subsequent sets of
    // 15024 frames.

    PdspChannelMapService channelMap("protoDUNETPCChannelMap_RCE_v4.txt", "protoDUNETPCChannelMap_FELIX_v4.txt");

    // const size_t frames_per_fragment=15024;

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    dune::FelixFrame* frame=f.fragment(0);

    // I'm using this to find the offline channel numbers for the link
    // which was dumped with dump_link, which has fiber 1 crate 6 slot
    // 0
    frame->set_fiber_no(1);
    frame->set_crate_no(6);
    frame->set_slot_no(0);

    const size_t apaNum=3;
    const size_t chanPerAPA=2560;
    const size_t offset=apaNum*chanPerAPA;

    // Change the value of all the collection channels to be their
    // offline number minus 3*2560 (because the FELIX APA is offline
    // 3). Set the induction channels to 0xfff so they show up if we
    // get them in the output
    for(size_t ich=0; ich<dune::FelixFrame::num_ch_per_frame; ++ich){
        unsigned int offline=getOfflineChannel(channelMap, frame, ich);
        assert(offline>offset);
        assert(offline-offset < (1<<12));
        frame->set_channel(ich, offline-offset);
    }

    // Get the collection channels into the output array using
    // (indirectly) get_frame_collection_adcs()
    // uint16_t* array=new uint16_t[dune::FelixFrame::num_ch_per_frame*frames_per_fragment];
    // fragment_frames_to_array_collection(frame, frames_per_fragment, array);
    std::cout << std::endl;

    // Keep a set of the channel numbers so we can tell whether we had any duplicates
    std::set<uint16_t> chs;

    RegisterArray<16> adcs=get_frame_all_adcs(frame);
    for(unsigned int i=0; i<16*SAMPLES_PER_REGISTER; ++i){
        uint16_t adc=adcs.uint16(i);
        if(chs.find(adc)!=chs.end()){
            std::cout << "Duplicate " << adc << " at index " << i << std::endl;
        }
        chs.insert(adc);
        // printf("% 5d % 5d\n", i, adcs.uint16(i));
    }
    // uint16_t min_channel=*chs.begin();
    printf("  i offln pln apa  wib femb chip asic asicchn asicln col/ind\n");
    for(unsigned int i=0; i<16*SAMPLES_PER_REGISTER; ++i){
        uint16_t adc=adcs.uint16(i);
        unsigned int offline=adc+offset;
        unsigned int apa=channelMap.APAFromOfflineChannel(offline);
        unsigned int wib=channelMap.WIBFromOfflineChannel(offline);
        unsigned int femb=channelMap.FEMBFromOfflineChannel(offline);
        // unsigned int fembChannel=channelMap.FEMBChannelFromOfflineChannel(offline);
        unsigned int chip=channelMap.ChipFromOfflineChannel(offline);
        //unsigned int chipChannel=channelMap.ChipChannelFromOfflineChannel(offline);
        unsigned int asic=channelMap.AsicFromOfflineChannel(offline);
        unsigned int asicChannel=channelMap.AsicChannelFromOfflineChannel(offline);
        unsigned int asicLink=channelMap.AsicLinkFromOfflineChannel(offline);
        unsigned int plane=channelMap.PlaneFromOfflineChannel(offline);
        const char* colind=offline%2560>1600 ? "c" : "i";
        printf("% 3d % 5d % 3d"
               "% 4d % 4d % 4d"
               " % 4d % 4d    % 4d   %4d    %s\n", i, offline, plane,
               apa, wib, femb,
               chip, asic, asicChannel,
               asicLink, colind);
        // if(i%16==15) printf("\n");
    }
}
