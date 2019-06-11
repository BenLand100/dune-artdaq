#include "PowerTwoHist.hh"

#include <iostream>

int main(int, char**)
{
    PowerTwoHist<10> hist;
    hist.fill(0);
    hist.fill(1);
    hist.fill(2);
    hist.fill(5);
    hist.fill(100);
    hist.fill(100);
    hist.fill(1<<12);

    for(size_t i=0; i<10; ++i){
        std::cout << i << "\t" << hist.binLo(i) << "\t" << hist.binHi(i) << "\t" << hist.bin(i) << std::endl;
    }
}
