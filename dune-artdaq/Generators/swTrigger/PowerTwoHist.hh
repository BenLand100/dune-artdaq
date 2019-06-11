#ifndef POWERTWOHIST_HH
#define POWERTWOHIST_HH

#include "stdint.h"
#include <cstring> // For memset

template<size_t N>
class PowerTwoHist
{
public:
    PowerTwoHist()
    {
        // Initialize all bin contents to zero
        memset(bins, 0, N*sizeof(uint64_t));
    }

    size_t fill(unsigned long x)
    {
        int bin=64-__builtin_clzl(x);
        if((size_t)bin>=N) bin=N-1;
        ++bins[bin];
        return bin;
    }

    uint64_t bin(size_t ibin) { return bins[ibin]; }
private:
    uint64_t bins[N];
};

#endif
