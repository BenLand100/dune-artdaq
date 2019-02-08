#include "../frame_expand.h"
#include <iostream>
#include <fstream>
#include <array>
#include "dune-raw-data/Overlays/FelixFormat.hh"

#include "immintrin.h"


//==============================================================================
int main(int, char**)
{
    std::ifstream is("../data/hundred.dat", std::ifstream::binary);
    char* buffer = new char[464];
    is.read (buffer,464);
    is.close();
    dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(buffer);
    frame->print();
    for(int adc=0; adc<8; ++adc){
        for(int ch=0; ch<8; ++ch){
            int val=8*(adc%2)+ch;
            frame->block(0).set_channel(adc, ch, val | (val<<4) | (val<<8));
        }
    }

    frame->block(0).printADCs();
    uint8_t* coldata_start_uint8=reinterpret_cast<uint8_t*>(&frame->block(0).segments[0]);
    for(int word=0; word<3; ++word){
        printf("Word %d: ", word);
        for(int i=0; i<8; ++i) printf("%02x ", coldata_start_uint8[8*word+i]);
        printf("\n");
    }

    // Set all the channels to their channel value
    for(unsigned ch=0; ch<dune::FelixFrame::num_ch_per_frame; ++ch){
        frame->set_channel(ch, ch);
    }
    // Set the collection channels to 0xfff, so they stand out (see find_collection.cpp for more details)
    for(unsigned ch=10; ch<22; ++ch) frame->set_channel(ch, 0xfff);
    for(unsigned ch=42; ch<54; ++ch) frame->set_channel(ch, 0xfff);

    frame->print();

    RegisterArray<8> collection_adcs=get_frame_collection_adcs(frame);
    for(int i=0; i<4; ++i){
        print256_as16(collection_adcs.ymm(2*i));   printf("  expanded coll 0\n\n"); 
        print256_as16(collection_adcs.ymm(2*i+1)); printf("  expanded coll 1\n\n"); 
    }
}
