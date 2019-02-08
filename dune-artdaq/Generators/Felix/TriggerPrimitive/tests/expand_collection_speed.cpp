#include <iostream>
#include <fstream>
#include <chrono>

#include "dune-raw-data/Overlays/FelixFormat.hh"
#include "../frame_expand.h"

int main(int, char**)
{
    std::ifstream is("../data/hundred.dat", std::ifstream::binary);
    char* buffer = new char[464*12];
    is.read(buffer,464*12);
    is.close();
    
    typedef std::chrono::high_resolution_clock clock;
    auto t0 = clock::now();

    size_t tot=0;
    size_t nrep=100000000;
    for(size_t j=0; j<nrep; ++j){
        for(int i=0; i<12; ++i){
            dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(buffer)+i;
            RegisterArray<8> collection_adcs=get_frame_collection_adcs(frame);
            tot+=(size_t)collection_adcs.data();
        }
    }

    std::cout << tot << std::endl;
    auto t1=clock::now();
    size_t ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    std::cout << nrep << " messages in " << ms << "ms = " << (double(nrep)/ms) << "kmsg/s" << std::endl;
}
