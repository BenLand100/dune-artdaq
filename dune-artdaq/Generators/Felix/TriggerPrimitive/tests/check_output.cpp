#include <map>
#include <set>
#include "../TriggerPrimitiveFinder.h"
#include "FrameFile.h"
#include "../process_naive.h"

struct Hit
{
    uint16_t chan, hit_start, hit_charge, hit_tover;
    bool operator<(const Hit& rhs) const {
        if(chan!=rhs.chan)             return chan<rhs.chan;
        if(hit_start!=rhs.hit_start)   return hit_start<rhs.hit_start;
        if(hit_charge!=rhs.hit_charge) return hit_charge<rhs.hit_charge;
        return hit_tover<rhs.hit_tover;
    }

    bool operator==(const Hit& rhs) const {
        return chan==rhs.chan           &&
            hit_start==rhs.hit_start    &&
            hit_charge==rhs.hit_charge  &&
            hit_tover==rhs.hit_tover;
    }
};

//======================================================================
std::set<Hit> get_avx2_hits(const uint16_t* input_loc)
{
    std::set<Hit> ret;
    uint16_t chan[16], hit_start[16], hit_charge[16], hit_tover[16];

    while(*input_loc!=MAGIC){
        for(int i=0; i<16; ++i) chan[i]       = *input_loc++;
        for(int i=0; i<16; ++i) hit_start[i]  = *input_loc++;
        for(int i=0; i<16; ++i) hit_charge[i] = *input_loc++;
        for(int i=0; i<16; ++i) hit_tover[i]  = *input_loc++;
        
        for(int i=0; i<16; ++i){
            if(hit_charge[i] && chan[i]!=MAGIC){
                ret.insert(Hit{chan[i], hit_start[i], hit_charge[i], hit_tover[i]});
            }
        }
    }
    return ret;
}

//======================================================================
std::set<Hit> get_naive_hits(const uint16_t* input_loc)
{
    std::set<Hit> ret;

    while(*input_loc!=MAGIC){
        uint16_t chan=*input_loc++;
        uint16_t hit_start=*input_loc++;
        uint16_t hit_charge=*input_loc++;
        uint16_t hit_tover=*input_loc++;
        ret.insert(Hit{chan, hit_start, hit_charge, hit_tover});
    }
    return ret;
}

//======================================================================
int main(int, char**)
{
    pthread_setname_np(pthread_self(), "main");

    mf::setStandAloneMessageThreshold({"INFO"});

    FrameFile f("/nfs/sw/work_dirs/phrodrig/felixcosmics.dat");
    char* fragment=reinterpret_cast<char*>(f.fragment(0));
    size_t length=FrameFile::frames_per_fragment*sizeof(dune::FelixFrame);
    size_t n_messages=length/NETIO_MSG_SIZE;

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
    uint16_t* primfind_dest_naive=new uint16_t[100000];
    uint16_t* primfind_dest_avx2=new uint16_t[100000];
    
    ProcessingInfo pi_naive(nullptr,
                            FRAMES_PER_MSG, // We'll just process one message
                            0, // First register
                            REGISTERS_PER_FRAME, // Last register
                            primfind_dest_naive,
                            taps_p, (uint8_t)taps.size(),
                            tap_exponent,
                            0);

    ProcessingInfo pi_avx2(nullptr,
                           FRAMES_PER_MSG, // We'll just process one message
                           0, // First register
                           REGISTERS_PER_FRAME, // Last register
                           primfind_dest_avx2,
                           taps_p, (uint8_t)taps.size(),
                           tap_exponent,
                           0);

    bool pass=true;
    for(size_t imessage=0; imessage<n_messages; ++imessage){
        SUPERCHUNK_CHAR_STRUCT* scs=reinterpret_cast<SUPERCHUNK_CHAR_STRUCT*>(fragment+imessage*NETIO_MSG_SIZE);
        RegisterArray<REGISTERS_PER_FRAME*FRAMES_PER_MSG> expanded=expand_message_adcs(*scs);
        MessageCollectionADCs* mcadc=reinterpret_cast<MessageCollectionADCs*>(expanded.data());
        if(imessage==0){
            pi_naive.setState(mcadc);
            pi_avx2.setState(mcadc);
        }
        pi_naive.input=mcadc;
        pi_avx2.input=mcadc;
        process_window_naive(pi_naive);
        process_window_avx2(pi_avx2);
        std::set<Hit> avx2_hits=get_avx2_hits(primfind_dest_avx2);
        std::set<Hit> naive_hits=get_naive_hits(primfind_dest_naive);
        if(avx2_hits!=naive_hits){
            pass=false;
            printf("message %ld. %ld avx2 hits, %ld naive_hits\n", imessage, avx2_hits.size(), naive_hits.size());
            std::vector<Hit> hit_union;
            std::set_union(avx2_hits.begin(), avx2_hits.end(),
                           naive_hits.begin(), naive_hits.end(),
                           std::back_inserter(hit_union));
            for(auto const& hit: hit_union){
                bool in_avx2=avx2_hits.find(hit)!=avx2_hits.end();
                bool in_naive=naive_hits.find(hit)!=naive_hits.end();
                if(!in_avx2)       printf("Naive only: ");
                else if(!in_naive) printf("AVX2 only:  ");
                else               printf("            ");
                printf("% 5d % 5d % 5d % 5d\n", hit.chan, hit.hit_start, hit.hit_charge, hit.hit_tover);
            }
            printf("-----------------------------------------------\n");
        }
    }

    if(pass) printf("naive and avx2 outputs matched! :-)\n");
    else     printf("naive and avx2 outputs differed! :-(\n");

    delete[] primfind_dest_naive;
    delete[] primfind_dest_avx2;

    return pass ? 0 : 1;
}

/* Local Variables:  */
/* mode: c++         */
/* c-basic-offset: 4 */
/* End:              */
