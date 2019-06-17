#include "netio/netio.hpp"
#include "../../NetioWIBRecords.hh"
#include "../frame_expand.h"
#include <thread>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdio>

int main(int argc, char** argv)
{
    // =============================================================================
    // Parse command line args
    int link, n_ticks;
    int n_channels;
    const char* outdir=argv[1];
    int n_repeats=1;
    if((argc!=5 && argc!=6) ||
       sscanf(argv[2],"%d",&link)!=1 ||
       sscanf(argv[3],"%d",&n_ticks)!=1 ||
       sscanf(argv[4],"%d",&n_channels)!=1 ||
       sscanf(argv[5],"%d",&n_repeats)!=1 ){
        std::cerr << "Usage: dump_link outdir link_number n_ticks n_channels [n_repeats]" << std::endl;
        exit(1);
    }


    // =============================================================================
    // Setup
    std::cout << "Setting up" << std::endl;
    const int n_msgs=n_ticks/FRAMES_PER_MSG;
    SUPERCHUNK_CHAR_STRUCT* ics=new SUPERCHUNK_CHAR_STRUCT[n_msgs];

    netio::context* context = new netio::context("fi_verbs");
    std::thread netio_bg_thread = std::thread( [&](){context->event_loop()->run_forever();} );

    netio::sockcfg cfg = netio::sockcfg::cfg(); 
    cfg(netio::sockcfg::ZERO_COPY);
    netio::subscribe_socket* sub_socket = new netio::subscribe_socket(context, cfg);

    const int port=12345+link;
    const std::string host="192.168.2.25";
    const int tag=0;

    netio::tag elink = tag;
    sub_socket->subscribe(elink, netio::endpoint(host, port));
    std::this_thread::sleep_for(std::chrono::microseconds(10000)); // This is needed... Why? :/

    std::cout << "Getting messages" << std::endl;
    // =============================================================================
    // Read messages
    for(int j=0; j<n_repeats; ++j){
        for(int i=0; i<n_msgs; ++i){
            netio::message msg;
            sub_socket->recv(std::ref(msg));
            if (msg.size()!=SUPERCHUNK_FRAME_SIZE) break;
            msg.serialize_to_usr_buffer((void*)&ics[i]);
        }
    }
    // =============================================================================
    // Cleanup

    std::cout << "Stopping" << std::endl;
    sub_socket->unsubscribe(elink, netio::endpoint(host, port));
    context->event_loop()->stop();
    netio_bg_thread.join();
    delete context;
    delete sub_socket;

    const dune::FelixFrame* framearray=reinterpret_cast<const dune::FelixFrame*>(&ics[0]);

    // =============================================================================
    // Write binary output file
    {
        char filename_bin[2000];
        sprintf(filename_bin, "%s/raw-channel-dump-link%d-%d-ticks-%d-channels-timestamp-0x%lx.dat", outdir, link, n_ticks, n_channels, framearray->timestamp());
        std::cout << "Writing binary output file " << filename_bin << std::endl;
        
        std::ofstream fout(filename_bin, std::ios::binary);
        // Put header information on the first line
        uint64_t timestamp=framearray->timestamp();
        uint8_t fiber_no=framearray->fiber_no();
        uint8_t crate_no=framearray->crate_no();
        uint8_t slot_no=framearray->slot_no();
        std::cout << "First timestamp is 0x" << std::hex << timestamp << std::dec << " fiber=" << (int)fiber_no << " crate=" << (int)crate_no << " slot=" << (int)slot_no << std::endl;
        fout.write((char*)&timestamp, sizeof(timestamp));
        fout.write((char*)&fiber_no, sizeof(fiber_no));
        fout.write((char*)&crate_no, sizeof(crate_no));
        fout.write((char*)&slot_no, sizeof(slot_no));
        uint16_t n_channels16=n_channels;
        fout.write((char*)&n_channels16, sizeof(n_channels16));


        for(unsigned int i=0; i<n_msgs*FRAMES_PER_MSG; ++i){
            if(i%1000000==0) std::cout << (i/1000000) << "m " << std::flush;
            const dune::FelixFrame* frame=framearray+i;
            RegisterArray<16> adcs=get_frame_all_adcs(frame);
            fout.write((char*)adcs.data(), n_channels*sizeof(uint16_t));
        }
        std::cout << std::endl;
    }

    // =============================================================================
    // Write text output file
    // {
    //     char filename[1000];
    //     sprintf(filename, "/dev/shm/raw-channel-dump-link%d-%d-ticks-%d-channels-timestamp-0x%lx.txt", link, n_ticks, n_channels, framearray->timestamp());
    //     std::cout << "Writing text output file " << filename << std::endl;
    //     std::ofstream fout(filename);
    //     // Put header information on the first line
    //     fout << std::hex << "0x" << framearray->timestamp() << " "  // timestamp in hex
    //          << std::dec << (framearray->timestamp()/50000000) << " " // timestamp in decimal seconds
    //          << ((int)framearray->fiber_no()) << " "
    //          << ((int)framearray->crate_no()) << " "
    //          << ((int)framearray->slot_no())  << " " << std::endl;
    //     for(unsigned int i=0; i<n_msgs*FRAMES_PER_MSG; ++i){
    //         if(i%1000000==0) std::cout << (i/1000000) << "m " << std::flush;
    //         const FelixFrame* frame=framearray+i;
    //         // RegisterArray<16> adcs=get_frame_all_adcs(frame);
    //         for(int j=0; j<n_channels; ++j){
    //             fout << frame->channel(j) << " ";
    //             // fout << adcs.uint16(j) << " ";
    //         }
    //         fout << std::endl;
    //     }
    //     std::cout << std::endl;
    // }

    delete ics;
}
