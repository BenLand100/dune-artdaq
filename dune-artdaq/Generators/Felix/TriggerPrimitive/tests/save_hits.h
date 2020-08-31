#ifndef SAVE_HITS_H
#define SAVE_HITS_H

#include <fstream>
#include <cstdio>
#include <iomanip>

#include "../constants.h"
#include "../frame_expand.h"

//======================================================================
void save_naive_hits(uint16_t* hits, const char* filename)
{
    std::ofstream fout(filename);
    uint16_t chan, hit_start, hit_charge, hit_tover;
    uint16_t* input_loc=hits;
    int nhit=0;
    while(true){
        chan        = collection_index_to_offline(*input_loc++);
        hit_start   = *input_loc++;
        hit_charge  = *input_loc++;
        hit_tover   = *input_loc++;
        // Four magic values indicates the end of hits
        if(chan==MAGIC && hit_start==MAGIC &&
           hit_charge==MAGIC && hit_tover==MAGIC) break;
        fout << std::setw(6) << chan << " "
             << std::setw(6) << hit_start << " "
             << std::setw(6) << hit_charge << " "
             << std::setw(6) << hit_tover << std::endl;
        ++nhit;
    }
    printf("naive: Saved %d hits\n", nhit);
}

//======================================================================
void save_avx2_hits(uint16_t* hits, const char* filename)
{
    std::ofstream fout(filename);
    uint16_t chan[16], hit_start[16], hit_charge[16], hit_tover[16];
    uint16_t* input_loc=hits;
    int nhit=0;

    while(true){
        for(int i=0; i<16; ++i) chan[i]       = collection_index_to_offline(*input_loc++);
        for(int i=0; i<16; ++i) hit_start[i]  = *input_loc++;
        for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
        for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;

        for(int i=0; i<16; ++i){
            if(hit_charge[i] && chan[i]!=MAGIC){
                fout << std::setw(6) << chan[i] << " "
                     << std::setw(6) << hit_start[i] << " "
                     << std::setw(6) << hit_charge[i] << " "
                     << std::setw(6) << hit_tover[i] << std::endl;
                ++nhit;
            }
        }
        if(*input_loc==MAGIC) break;
    }
    printf("intrinsic: Saved %d hits\n", nhit);
}

#endif
