#include <map>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"

int main(int argc, char** argv)
{
    pthread_setname_np(pthread_self(), "main");
    int n_repeats=10;
    if(argc>=2)  sscanf(argv[1],"%d",&n_repeats);

    mf::setStandAloneMessageThreshold({"ERROR"});

    // std::this_thread::sleep_for(std::chrono::seconds(1));
    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(dune::FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

    printf("n_messages = %ld, n_repeats= %d, NETIO_MSG_SIZE= %ld, queue size = %ld\n",
           n_messages, n_repeats, NETIO_MSG_SIZE, NETIO_MSG_SIZE*n_messages*n_repeats);

    fhicl::ParameterSet ps;
    ps.put<std::string>("zmq_hit_send_connection", "tcp://*:54321");
    ps.put<uint32_t>("window_offset", 500);
    TriggerPrimitiveFinder* tpf=new TriggerPrimitiveFinder(ps);
    auto t0=std::chrono::steady_clock::now();

    std::cout << "Running " << n_repeats << " repeats" << std::endl;
    for(int irepeat=0; irepeat<n_repeats; ++irepeat){
        for(size_t imessage=0; imessage<n_messages; ++imessage){
            SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
            while(!tpf->addMessage(*scs)){
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    // This bit is to test that the "behindness" detection in
    // TriggerPrimitiveFinder is working: with nrepeats=1000, we'll
    // end up behind in the loop above. We wait for the processing
    // thread to catch up, then send another message, which should get
    // processed straight away, and take the processing thread out of
    // the lateness state

    // std::this_thread::sleep_for(std::chrono::seconds(8));
    // SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment);
    // tpf->addMessage(*scs);
    
    delete tpf; // To force the destructor to run
    auto t1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    printf("Processed %.0fms of data in %ldms. Ratio %.0f%%\n", ms_processed, ms, 100.*float(ms)/ms_processed);
}
