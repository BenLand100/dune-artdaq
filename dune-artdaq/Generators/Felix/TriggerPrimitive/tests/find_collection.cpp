#include <iostream>
#include <fstream>

#include "dune-raw-data/Overlays/FelixFormat.hh"

int main(int, char**)
{
    // Get the "stream number" of the collection channels with this
    // command, using the fact that the 5th column is the stream
    // number, and the 13th column is the offline number, which has
    // "number within APA" of 1600 or greater for collection channels:
    //
    // awk '$13%2560>=1600 {print $5}' protoDUNETPCChannelMap_FELIX_v4.txt | sort -nu
    //
    // There are 256 channels in a WIB frame, and according to
    // MakePdspChannelMap_v4.C in dune-raw-data, the second 128
    // channels are in the same pattern as the first 128.
    //
    // The result of all that is that the following ranges of Felix
    // frame channel are collection wires (inclusive):
    //
    // 10-21, 42-53, 74-85, 106-117
    //
    // and the same with 128 added:
    // 
    // 138-149, 170-181, 202-213, 234-245
    std::ifstream is("../data/hundred.dat", std::ifstream::binary);
    char* buffer = new char[464];
    is.read(buffer,464);
    is.close();
    dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(buffer);
    // Set every channel's value to its channel number, so I can see
    // what the numbering scheme is
    for(unsigned ch=0; ch<dune::FelixFrame::num_ch_per_frame; ++ch){
        frame->set_channel(ch, ch);
    }
    // Set the collection channels to 0xfff, so they show up
    for(unsigned ch=10; ch<22; ++ch) frame->set_channel(ch, 0xfff);
    for(unsigned ch=42; ch<54; ++ch) frame->set_channel(ch, 0xfff);
    frame->print();
}
