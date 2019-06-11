#include "PowerTwoHist.hh"

#include <iostream>

int main(int, char**)
{
    PowerTwoHist<10> hist;
    hist.fill(1);
    hist.fill(5);
    hist.fill(1<<12);

    for(size_t i=0; i<10; ++i){
        std::cout << i << "\t" << hist.bin(i) << std::endl;
    }
}
