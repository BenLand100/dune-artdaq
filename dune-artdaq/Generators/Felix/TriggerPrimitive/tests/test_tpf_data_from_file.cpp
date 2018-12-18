#include <map>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"

int main(int, char**)
{
    // auto t0=std::chrono::steady_clock::now();
    mf::setStandAloneMessageThreshold({"DEBUG"});
    auto tpf=std::make_unique<TriggerPrimitiveFinder>(50000, 128, 2);

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

    for(size_t imessage=0; imessage<n_messages; ++imessage){
        SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
        tpf->addMessage(*scs);
        if(imessage%2048==0) std::this_thread::sleep_for(std::chrono::microseconds(1024));
    }
    // auto t1=std::chrono::steady_clock::now();
    // auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    // auto ms_processed=1000*N*FRAMES_PER_MSG/2e6;
    // std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;
    std::cout << "n_messages: " << tpf->getNMessages() << " n_windows: " << tpf->getNWindowsProcessed() << " n_primitives: " << tpf->getNPrimitivesFound() << std::endl;

}
