#include "netio/netio.hpp"
#include "../../NetioWIBRecords.hh"
#include "../frame_expand.h"
#include "../TriggerPrimitiveFinder.h"
#include "../PdspChannelMapService.h"
#include "frames2array.h"
#include <thread>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstdio>

int main(int argc, char** argv)
{
    mf::setStandAloneMessageThreshold({"INFO"});

    // =============================================================================
    // Parse command line args
    int link, n_msgs;
    std::string outfilebase("");
    if(argc==4) outfilebase=argv[3];
    if((argc!=3 && argc!=4) ||
       sscanf(argv[1],"%d",&link)!=1 ||
       sscanf(argv[2],"%d",&n_msgs)!=1){
        std::cerr << "Usage: run_tpf_on_link link_number n_msgs [output-file-base]" << std::endl;
        exit(1);
    }

    // =============================================================================
    // Setup
    std::cout << "Setting up with link=" << link << " for " << n_msgs << " messages" << std::endl;
    SUPERCHUNK_CHAR_STRUCT* ics=new SUPERCHUNK_CHAR_STRUCT[n_msgs];

    TriggerPrimitiveFinder* tpf=new TriggerPrimitiveFinder();
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
    uint64_t first_ts=0;
    for(int i=0; i<n_msgs; ++i){
        netio::message msg;
        sub_socket->recv(std::ref(msg));
        if (msg.size()!=SUPERCHUNK_FRAME_SIZE) break;
        msg.serialize_to_usr_buffer((void*)&ics[i]);
        tpf->addMessage(ics[i]);
        if(i==0){
            dune::FelixFrame* frame=reinterpret_cast<dune::FelixFrame*>(ics);
            first_ts=frame->timestamp();
        }
    }

    // =============================================================================
    // Cleanup

    std::cout << "Stopping subscriber..." << std::flush;
    sub_socket->unsubscribe(elink, netio::endpoint(host, port));
    std::cout << "unsubscribed... " << std::flush;
    context->event_loop()->stop();
    std::cout << "stopped netio evt loop... " << std::flush;
    netio_bg_thread.join();
    std::cout << "joined netio thread" << std::endl;

    const int clocksPerTPCTick=25;
    uint64_t last_ts=first_ts+clocksPerTPCTick*n_msgs*FRAMES_PER_MSG/2;
    std::cout << "Retrieving hits from getHitsForWindow(" << first_ts << ", " << last_ts << ")..." << std::flush;
    std::vector<dune::TriggerPrimitive> hits=tpf->getHitsForWindow(first_ts, last_ts);
    std::cout << " got " << hits.size() << " of them" << std::endl;
    std::cout << "Finished listening to messages. Waiting for processing to finish" << std::endl;
    delete tpf;
    std::cout << "Processing finished" << std::endl;

    const dune::FelixFrame* framearray=reinterpret_cast<const dune::FelixFrame*>(&ics[0]);

    // =============================================================================
    // Write output files
    PdspChannelMapService channelMap("protoDUNETPCChannelMap_RCE_v4.txt", "protoDUNETPCChannelMap_FELIX_v4.txt");

    if(outfilebase!=""){
        char filename[2000];
        sprintf(filename, "%s-raw.txt", outfilebase.c_str());
        std::cout << "Writing raw output file " << filename << std::endl;
        
        std::ofstream fout(filename);

        fout << "0 ";
        unsigned int ch_to_offline[dune::FelixFrame::num_ch_per_frame];
        for(unsigned int ich=0; ich<dune::FelixFrame::num_ch_per_frame; ++ich){
            ch_to_offline[ich]=getOfflineChannel(channelMap, framearray, ich);
            if(ch_to_offline[ich]%2560>=1600){
                fout << ch_to_offline[ich] << " ";
            }
        }
        fout << std::endl;

        for(unsigned int i=0; i<n_msgs*FRAMES_PER_MSG; ++i){
            const dune::FelixFrame* frame=framearray+i;
            fout << frame->timestamp() << " ";
            for(unsigned int ich=0; ich<dune::FelixFrame::num_ch_per_frame; ++ich){
                if(ch_to_offline[ich]%2560>=1600){
                    fout << frame->channel(ich) << " ";
                }
            }
            fout << std::endl;
        }
        std::cout << std::endl;

        char filename_hits[2000];
        sprintf(filename_hits, "%s-hits.txt", outfilebase.c_str());
        std::cout << "Writing hit output file " << filename_hits << std::endl;
        
        std::ofstream fout_hits(filename_hits);
        for(auto const& hit: hits){
            fout_hits << hit.startTime << "\t" << hit.channel << "\t" << hit.charge << "\t" << hit.timeOverThreshold << std::endl;
        }
    }


    delete context;
    delete sub_socket;
    delete[] ics;

}
