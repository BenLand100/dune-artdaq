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
    int seconds_to_run;
    if(argc!=4 ||
       sscanf(argv[1],"%d",&link)!=1 ||
       sscanf(argv[2],"%d",&n_ticks)!=1 ||
       sscanf(argv[3],"%d",&seconds_to_run)!=1){
        std::cerr << "Usage: dump_link link_number n_ticks seconds_to_run" << std::endl;
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
    auto t0=std::chrono::steady_clock::now();
    while(std::chrono::steady_clock::now()-t0 < std::chrono::seconds(seconds_to_run)){
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

    delete[] ics;
}
