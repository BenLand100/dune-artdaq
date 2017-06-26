//################################################################################
//# /*
//#        tsetup.cpp
//#
//#  Note the makefile makes an executable called tsetup, because setup is
//#  already a system command that we don't want to hide.
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//# */
//################################################################################

// Test compile with: g++ -c -std=c++11 -I /opt/cactus/include  tsetup.cpp

#include "uhal/uhal.hpp"
#include "I2CuHal.hpp"
#include "si5344.hpp"
#include <chrono>    // For std::chrono::milliseconds
#include <thread>    // For std::thread::sleep_for

// Slight change from the Python.  It rolls hw_tx and hw_rx into a 
// list and then loops over those two twice.   We have made hwinit() 
// and hwstatus() and call them both twice (once each for hw_tx and hw_rx)
// in main (at the bottom here) in the same sequence as the python.
// (The problem is making a std::vector<HwInterface&> and initialising it) 

// This has routines of the 'useful' sequences of commands etc in it, e.g. hwinit and hwstatus
#include "TimingSequence.hpp"

int main() {

// uhal::setLogLevelTo(uhal::Error());
uhal::setLogLevelTo(uhal::Notice());
uhal::ConnectionManager manager("file://connections.xml");

//--- Check if file 0000 goes with TX and 0003 with RX or other way round)

uhal::HwInterface hw_rx = manager.getDevice("DUNE_FMC_RX");
TimingSequence::hwinit(hw_rx,"SI5344/PDTS0003.txt");

TimingSequence::hwstatus(hw_rx);

return 0;
}

