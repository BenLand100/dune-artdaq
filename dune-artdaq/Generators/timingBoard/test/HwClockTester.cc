#include <iostream>

#include "../HwClockSubscriber.hh"

int main(int argc, char const *argv[])
{
    std::cout << argc << "  " << sizeof(*argv) << std::endl;

    dune::HwClockSubscriber sub("tcp://localhost:5555");

    std::cout << "Connecting" << std::endl;
    sub.connect();
    std::cout << "Connected" << std::endl;

    std::cout << sub.receiveHardwareTimeStamp() << std::endl;

    return 0;
}