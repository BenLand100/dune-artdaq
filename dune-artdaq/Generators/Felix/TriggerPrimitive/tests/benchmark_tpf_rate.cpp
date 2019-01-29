#include <map>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"

int main(int argc, char** argv)
{
    pthread_setname_np(pthread_self(), "main");
    int nthreads=1;
    int n_repeats=1000;
    if(argc>=2)  sscanf(argv[1],"%d",&nthreads);
    if(argc>=3)  sscanf(argv[2],"%d",&n_repeats);
    // auto t0=std::chrono::steady_clock::now();
    mf::setStandAloneMessageThreshold({"ERROR"});
    const int window_size=128;
    auto tpf=std::make_unique<TriggerPrimitiveFinder>(50000, window_size, nthreads);

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

    auto t0=std::chrono::steady_clock::now();

    std::cout << "Running " << n_repeats << " repeats with " << nthreads << " threads" << std::endl;
    for(int irepeat=0; irepeat<n_repeats; ++irepeat){
        for(size_t imessage=0; imessage<n_messages; ++imessage){
            SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
            tpf->addMessage(*scs);
            // if(imessage%2048==0) std::this_thread::sleep_for(std::chrono::microseconds(1024));
        }
        // auto t00=std::chrono::steady_clock::now();
        if(irepeat%window_size==0) tpf->waitForJobs();
        // auto t11=std::chrono::steady_clock::now();
        // auto us=std::chrono::duration_cast<std::chrono::microseconds>(t11-t00).count();
    }
    auto t1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;
    std::cout << "n_messages: " << tpf->getNMessages() << " n_windows: " << tpf->getNWindowsProcessed() << " n_primitives: " << tpf->getNPrimitivesFound() << std::endl;

}
