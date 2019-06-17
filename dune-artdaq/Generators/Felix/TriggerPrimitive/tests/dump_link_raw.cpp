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
    int do_expand=0;
    const char* outdir=argv[1];
    if((argc!=4 && argc!=5) ||
       sscanf(argv[2],"%d",&link)!=1 ||
       sscanf(argv[3],"%d",&n_ticks)!=1 ||
       (argc==5 && sscanf(argv[4],"%d",&do_expand)!=1)){
        std::cerr << "Usage: dump_link_raw outdir link_number n_ticks [do_expand]" << std::endl;
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
    const std::string host="192.168.2.26";
    const int tag=0;

    netio::tag elink = tag;
    sub_socket->subscribe(elink, netio::endpoint(host, port));
    std::this_thread::sleep_for(std::chrono::microseconds(10000)); // This is needed... Why? :/

    std::cout << "Getting messages" << std::endl;
    // =============================================================================
    // Read messages
    for(int i=0; i<n_msgs; ++i){
        netio::message msg;
        sub_socket->recv(std::ref(msg));
        if (msg.size()!=SUPERCHUNK_FRAME_SIZE) break;
        msg.serialize_to_usr_buffer((void*)&ics[i]);
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

    char filename_bin[2000];
    sprintf(filename_bin, "%s/%s-frame-dump-link%d-%d-ticks-timestamp-0x%lx.dat", outdir, do_expand ? "expanded" : "raw", link, n_ticks, framearray->timestamp());
    std::cout << "Writing binary output file " << filename_bin << std::endl;
        
    std::ofstream fout(filename_bin, std::ios::binary);
    if(do_expand){
        for(int i=0; i<n_msgs; ++i){
            RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(ics[i]);
            fout.write((char*)&expanded, sizeof(expanded));
        }
    }
    else{
        fout.write((char*)ics, n_msgs*sizeof(SUPERCHUNK_CHAR_STRUCT));
    }

    delete ics;
}
