#include <map>
#include "../TriggerPrimitiveFinder.h"

int main(int, char**)
{
    auto t0=std::chrono::steady_clock::now();
    mf::setStandAloneMessageThreshold({"DEBUG"});
    auto tpf=std::make_unique<TriggerPrimitiveFinder>(50000, 128, 4);
    SUPERCHUNK_CHAR_STRUCT* scs=(SUPERCHUNK_CHAR_STRUCT*)calloc(1, sizeof(SUPERCHUNK_CHAR_STRUCT));
    const int N=2000000;
    for(int i=0; i<N; ++i){
        tpf->addMessage(*scs);
        if(i%2048==0) std::this_thread::sleep_for(std::chrono::microseconds(1024));
    }
    auto t1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    auto ms_processed=1000*N*FRAMES_PER_MSG/2e6;
    std::cout << "Processed " << ms_processed << "ms of data in " << ms << "ms" << std::endl;
    free(scs);
}
