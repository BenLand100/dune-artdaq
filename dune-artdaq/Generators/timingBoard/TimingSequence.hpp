#ifndef TIMINGSEQ_HPP
#define TIMINGSEQ_HPP

#include "uhal/uhal.hpp"

//################################################################################
//# /*
//#        TimingSequence
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//#  It was a sequence of calls to I2CUHAL etc to configure and set up the
//#  timing board.
//# */
//################################################################################

namespace TimingSequence {
  void bufstatus(uhal::HwInterface& hw);
  void hwinit(uhal::HwInterface& hw, uint32_t init_softness);
  void hwstatus(uhal::HwInterface& hw);
}

#endif

