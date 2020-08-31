#include <map>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"

int main(int argc, char** argv)
{
    pthread_setname_np(pthread_self(), "main");
    int n_repeats=10;
    if(argc>=2)  sscanf(argv[1],"%d",&n_repeats);

    mf::setStandAloneMessageThreshold({"INFO"});

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(dune::FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

    auto t0=std::chrono::steady_clock::now();

    MessageCollectionADCs* all_mcadc=new MessageCollectionADCs[n_messages];
    for(size_t imessage=0; imessage<n_messages; ++imessage){
        SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        all_mcadc[imessage]=*mcadc;
    }

    const uint8_t tap_exponent=6;
    const int multiplier=1<<tap_exponent; // 64
    std::vector<int16_t> taps=firwin_int(7, 0.1, multiplier);
    taps.push_back(0); // Make it 8 long so it's a power of two

    // TODO: ProcessingInfo points to but doesn't copy the taps it's
    // passed, so we make a long-lived copy here. We ought to not leak
    // this (and also do it properly)
    int16_t* taps_p=new int16_t[taps.size()];
    for(size_t i=0; i<taps.size(); ++i) taps_p[i]=taps[i];

    // Temporary place to stash the hits
    uint16_t* primfind_dest=new uint16_t[100000];
    
    // Place to put the hits more tidily for later retrieval
    std::deque<dune::TriggerPrimitive> primitive_queue;
      
    ProcessingInfo pi(nullptr,
                      FRAMES_PER_MSG, // We'll just process one message
                      0, // First register
                      REGISTERS_PER_FRAME, // Last register
                      primfind_dest,
                      taps_p, (uint8_t)taps.size(),
                      tap_exponent,
                      0, // nhits
                      0); // absTimeModNTAPS

    bool first=true;
    std::cout << "Running " << n_repeats << " repeats" << std::endl;
    for(int irepeat=0; irepeat<n_repeats; ++irepeat){
        for(size_t imessage=0; imessage<n_messages; ++imessage){
            MessageCollectionADCs* mcadc=all_mcadc+imessage;
            if(first){
                pi.setState(mcadc);
                first=false;
            }
            pi.input=mcadc;
            // "Empty" the list of hits
            *primfind_dest=MAGIC;
            // Do the processing
            process_window_avx2(pi);
        }
    }
    auto t1=std::chrono::steady_clock::now();
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
    auto ms_processed=1000*n_repeats*n_messages*FRAMES_PER_MSG/2e6;
    printf("Processed %.0fms of data in %ldms. Ratio %.0f%%\n", ms_processed, ms, 100.*float(ms)/ms_processed);
}
