#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
    if(argc!=2){
        std::cerr << "Usage: read_raw_file filename" << std::endl;
        exit(1);
    }

    std::ifstream fin(argv[1], std::ios::binary);
    // The input file contains the 32-bit timestamp, followed by 8-bit
    // fiber, crate and slot numbers. Then there's a 16-bit
    // number-of-channels-saved, followed by the actual ADC values,
    // stored as uint16_t. All channels are stored for a given time
    // tick, followed by all channels for the next time tick, and so
    // on
    uint64_t timestamp;
    uint8_t fiber_no, crate_no, slot_no;
    uint16_t n_channels;
    fin.read((char*)&timestamp, sizeof(timestamp));
    fin.read((char*)&fiber_no,  sizeof(fiber_no));
    fin.read((char*)&crate_no,  sizeof(crate_no));
    fin.read((char*)&slot_no,   sizeof(slot_no));
    fin.read((char*)&n_channels,   sizeof(n_channels));

    std::cout << timestamp << " " << ((int)fiber_no) << " " << ((int)crate_no) << " " << ((int)slot_no) << std::endl;

    uint16_t adcs[16*16]; // Size to the maximum possible number of channels
    while(!fin.eof()){
        fin.read((char*)adcs, n_channels*sizeof(uint16_t));
        if(!fin) break;
        // At this point, adcs[0]-adcs[n_channels] are filled with the values of the channels at the current TPC tick
        for(int j=0; j<n_channels; ++j){
            std::cout << adcs[j] << " ";
        }
        std::cout << std::endl;
    }
}
