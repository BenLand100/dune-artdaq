#include "frames2array.h"
#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "FrameFile.h"

#include <fstream>
#include <iostream>
#include <cassert>
#include <cstdio>
#include <numeric> // For iota()
#include <algorithm> // For sort()

// argsort, from https://stackoverflow.com/questions/1577475/c-sorting-and-keeping-track-of-indexes
template <typename T>
std::vector<size_t> sort_indexes(const std::vector<T> &v) {

    // initialize original index locations
    std::vector<size_t> idx(v.size());
    std::iota(idx.begin(), idx.end(), 0);

    // sort indexes based on comparing values in v
    std::sort(idx.begin(), idx.end(),
         [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

    return idx;
}

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

    PdspChannelMapService channelMap("../data/protoDUNETPCChannelMap_RCE_v4.txt", "../data/protoDUNETPCChannelMap_FELIX_v4.txt");

    const size_t frames_per_fragment=15024;

    FrameFile f("/data/lar/dunedaq/rodrigues/protodune-noise/felixcosmics.dat");
    // FrameFile f("/data/lar/dunedaq/rodrigues/protodune-noise/frames-from-real-link.dat");
    for(size_t ifragment=0; ifragment<f.num_fragments(); ++ifragment){
        dune::FelixFrame* frame=f.fragment(ifragment);

        uint16_t* array=new uint16_t[dune::FelixFrame::num_ch_per_frame*frames_per_fragment];
        std::vector<uint16_t> chans;
        fragment_frames_to_array(frame, frames_per_fragment, array, chans);
        assert(chans.size()==dune::FelixFrame::num_ch_per_frame);
        // Save everything from the fragment to file
        std::string filename("/data/lar/dunedaq/rodrigues/protodune-noise/felixcosmics-fragment");
        filename+=std::to_string(ifragment);
        filename+=std::string(".txt");
        std::ofstream fout(filename.c_str());
        int printed=0;
        std::cout << "From everything:" << std::endl;
        std::vector<size_t> chan_sort_indexes=sort_indexes(chans);

        for(size_t ich=0; ich<chans.size(); ++ich){
            size_t chan_index=chan_sort_indexes[ich];
            uint16_t channel=chans[chan_index];
            fout << channel << " ";
            for(size_t itime=0; itime<frames_per_fragment; ++itime){
                fout << array[dune::FelixFrame::num_ch_per_frame*itime+chan_index] << " ";
            }
            fout << std::endl;

            // Print the first few values in the collection channels to compare to what's below
            if((++printed<30) && channel%2560>1600){
                printf("ch % 5d, plane % 3d: ", channel, channelMap.PlaneFromOfflineChannel(channel));

                for(size_t itime=0; itime<5; ++itime){
                    printf("% 5d ", array[dune::FelixFrame::num_ch_per_frame*itime+chan_index]);
                }
                std::cout << std::endl;
            }
        }

        fragment_frames_to_array_collection(frame, frames_per_fragment, array);
        // std::ofstream fout("test-coll.txt");
        std::cout << std::endl;
        std::cout << "From collection only:" << std::endl;
        printed=0;
        for(size_t ich=0; ich<16*8; ++ich){
            if(++printed>30) break;
            printf("ch % 5ld: ", ich);
            for(size_t itime=0; itime<5; ++itime){
                printf("% 5d ", array[itime*16*8+ich]);
            }
            std::cout << std::endl;
        }
        delete[] array;
    } // end loop over fragments

}
