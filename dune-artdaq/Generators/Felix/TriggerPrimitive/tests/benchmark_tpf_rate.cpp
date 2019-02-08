#include <map>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"

int main(int argc, char** argv)
{
    pthread_setname_np(pthread_self(), "main");
    int n_repeats=10;
    if(argc>=2)  sscanf(argv[1],"%d",&n_repeats);
    // auto t0=std::chrono::steady_clock::now();
    mf::setStandAloneMessageThreshold({"ERROR"});
    TriggerPrimitiveFinder* tpf=new TriggerPrimitiveFinder;

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(dune::FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

    auto t0=std::chrono::steady_clock::now();

    std::cout << "Running " << n_repeats << " repeats" << std::endl;
    for(int irepeat=0; irepeat<n_repeats; ++irepeat){
        for(size_t imessage=0; imessage<n_messages; ++imessage){
            SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
            tpf->addMessage(*scs);
        }
    }
    delete tpf; // To force the destructor to run
    auto t1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;
    // std::cout << "n_messages: " << tpf->getNMessages() << " n_windows: " << tpf->getNWindowsProcessed() << " n_primitives: " << tpf->getNPrimitivesFound() << std::endl;

}
