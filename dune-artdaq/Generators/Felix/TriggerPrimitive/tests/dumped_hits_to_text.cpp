#include "ptmp/api.h"
#include "czmq.h"

#include "CLI11.hpp"

#include "dune-artdaq/Generators/swTrigger/ptmp_util.hh"

#include <cstdio>

// Every message data is assumed to be prefixed by a 64 bit unsigned
// int holding the size of the message data in bytes.
zmsg_t* read_msg(FILE* fp)
{
    size_t size=0;
    int nread = fread(&size, sizeof(size_t), 1, fp);
    if (nread != 1) {
        return NULL;
    }
    zchunk_t* chunk = zchunk_read(fp, size);
    zframe_t* frame = zchunk_pack(chunk);
    zchunk_destroy(&chunk);
    zmsg_t* msg = zmsg_decode(frame);
    zframe_destroy(&frame);
    return msg;
}

int main(int argc, char** argv)
{
    CLI::App app{"Print dumped hits"};

    std::string input_file{"/nfs/sw/work_dirs/phrodrig/hit-dumps/run8567/FELIX_BR_508.dump"};
    app.add_option("-f", input_file, "Input file", true);

    int nhits=1000;
    app.add_option("-n", nhits, "number of hits to print (-1 for no limit)", true);

    CLI11_PARSE(app, argc, argv);
    
    zsock_t* sender=zsock_new(ZMQ_PUB);
    zsock_bind(sender, "inproc://hits");
    
    ptmp::TPReceiver receiver(ptmp_util::make_ptmp_socket_string("SUB", "connect", {"inproc://hits"}));
    FILE* fp=fopen(input_file.c_str(), "r");
    zmsg_t* msg=NULL;
    int counter=0;
    while((msg = read_msg(fp))){
        zsock_send(sender, "m", msg);
        ptmp::data::TPSet tpset;
        receiver(tpset);
        for(auto const& tp: tpset.tps()){
            printf("0x%06x %d %ld %d %d\n", tpset.detid(), tp.channel(), tp.tstart(), tp.adcsum(), tp.tspan()); 
            ++counter;
        }
        if(nhits>0 && counter>nhits) break;
    }

    zsock_destroy(&sender);
}
