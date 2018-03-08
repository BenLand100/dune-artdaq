//################################################################################
//# /*
//#        TimingSequence.cpp
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The python version was created by the Bristol High Energy Physics group
//#  It was a sequence of calls to I2CUHAL etc to configure and set up the
//#  timing board.
//# */
//################################################################################
//
// Test compile with: g++ -c -std=c++11 -I /opt/cactus/include  setup.cpp

#include "uhal/uhal.hpp"
#include "I2CuHal.hpp"
#include "si5344.hpp"
#include <chrono>    // For std::chrono::milliseconds
#include <thread>    // For std::thread::sleep_for
#include "TimingSequence.hpp"

// Slight change from the Python.  It rolls hw_tx and hw_rx into a 
// list and then loops over those two twice.   We have made hwinit() 
// and hwstatus() and call them both twice (once each for hw_tx and hw_rx)
// in main (at the bottom here) in the same sequence as the python.
// (The problem is making a std::vector<HwInterface&> and initialising it) 

#define N_SCHAN 1
#define N_TYPE 5

// Keep VERBOSE undefined to use the DAQ log message facility (and have only a few messages) and define
// it to use std::cout only and have all the prints from the python scripts.
//#define VERBOSE

#ifndef VERBOSE
#include "messagefacility/MessageLogger/MessageLogger.h"  // For log messages
#include "dune-artdaq/DAQLogger/DAQLogger.hh"             // For log messsages
#endif

void TimingSequence::bufstatus(uhal::HwInterface& hw) {
    //Superseeded Sep2017.   uhal::ValVector<uint32_t> m_t = hw.getNode("master.global.tstamp").readBlock(2);
    uhal::ValWord<uint32_t> m_e = hw.getNode("master.partition0.evtctr").read();
    //uhal::ValVector<uint32_t> e_t = hw.getNode("endpoint.tstamp").readBlock(2);
    //uhal::ValWord<uint32_t> e_e = hw.getNode("endpoint.evtctr").read();
    uhal::ValWord<uint32_t> m_c = hw.getNode("master.partition0.buf.count").read();
    //uhal::ValWord<uint32_t> e_c = hw.getNode("endpoint.buf.count").read();
    uhal::ValWord<uint32_t> m_stat = hw.getNode("master.global.csr.stat").read();
    uhal::ValWord<uint32_t> p_stat = hw.getNode("master.partition0.csr.stat").read();
    //uhal::ValWord<uint32_t> e_stat = hw.getNode("endpoint.csr.stat").read();
    uhal::ValVector<uint32_t> s_a = hw.getNode("master.scmd_gen.actrs").readBlock(N_SCHAN);
    uhal::ValVector<uint32_t> s_r = hw.getNode("master.scmd_gen.rctrs").readBlock(N_SCHAN);
    uhal::ValVector<uint32_t> p_a = hw.getNode("master.partition0.actrs").readBlock(N_TYPE);
    uhal::ValVector<uint32_t> p_r = hw.getNode("master.partition0.rctrs").readBlock(N_TYPE);
    //uhal::ValVector<uint32_t> e_a = hw.getNode("endpoint.ctrs").readBlock(N_TYPE);
    hw.dispatch();
#ifdef VERBOSE
    std::cout << "hw.id() = " << hw.id() << std::endl;
    std::cout << std::hex << "m_stat 0x"   << m_stat << std::dec << std::endl;
    std::cout << std::hex << "/ p_stat 0x" << p_stat 
//                          << "/ e_stat 0x" << e_stat 
                                           << std::dec << std::endl;

    std::cout << std::hex 
//                          << "m_t 0x" << int(m_t[1]) << " " << int(m_t[0]) 
                          << " m_e 0x" <<  m_e << " m_c 0x" << m_c << std:: dec << std::endl;
//     print "e_t / e_e / e_c:", hex(int(e_t[0]) + (int(e_t[1]) << 32)), hex(e_e), hex(e_c)
    std::cout << std::hex << "s_a 0x" << s_a[0] << std::dec << std::endl;
    std::cout << std::hex << "s_r 0x" << s_r[0] << std::dec << std::endl;
    std::cout << std::hex << "p_a 0x" << p_a[0] << " 0x" << p_a[1] << " 0x" << p_a[2] 
                             << " 0x" << p_a[3] << " 0x" << p_a[4] << std::dec << std::endl;
    std::cout << std::hex << "p_r 0x" << p_r[0] << " 0x" << p_r[1] << " 0x" << p_r[2] 
                             << " 0x" << p_r[3] << " 0x" << p_r[4] << std::dec << std::endl;
#else
    if (0 == m_stat + p_stat + m_e + m_c + s_a[0] + s_r[0] + p_a[0] + p_r[0]) 
           { /* Defeat compiler unused variable warning */ }
//    if (0 = mt[1]) { /* and again */ }
#endif
}

void TimingSequence::hwinit(uhal::HwInterface& hw, uint32_t init_softness) {  

  // Does the initialisation sequence.   init_softness allows a partial reset for testing.
  // init_softness = 0 Do everything: soft_rst, pll_rst, I2C, en_lines, promread, wr_clk_list, freq_meas, sfp_tx_dif, ctrl.rst
  // init_softness = 1            Do:                    I2C,           promread, wr_clk_list, freq_meas, sfp_tx_dif, ctrl.rst
  // init_softness = 2            Do:                    I2C,           promread,              freq_meas, sfp_tx_dif, ctrl.rst
  // init_softness = 3            Do:                    I2C,           promread,              freq_meas

#ifndef VERBOSE
  std::string instancename = "Timing";
#endif

  if (init_softness < 1) {  // soft_rst:   Does not take clock away, but sets the registers to 'factory' defaults
    std::cout << "hw.id() = " << hw.id() << std::endl;
    uhal::ValWord<uint32_t> reg = hw.getNode("io.csr.stat").read();
    hw.getNode("io.csr.ctrl.soft_rst").write(1);
    hw.dispatch();
#ifdef VERBOSE
    std::cout << "reg = 0x" << std::hex << reg << std::dec << std::endl;
#else
    if (reg) { /* Defeat compiler unused variable warning */ }
#endif 
  }
// write to 'nuke' to completely reset.   This resets so much uhal will not get a reply.   use with real caution.

  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  if (init_softness < 1) {   // pll_rst.   This will remove the 50MHz clock (end point will see this and say not ready)
      hw.getNode("io.csr.ctrl.pll_rst").write(1);
      hw.dispatch();
      hw.getNode("io.csr.ctrl.pll_rst").write(0);
      hw.dispatch();
  }

  I2CCore uid_I2C = I2CCore(hw, 10, 5, "io.uid_i2c", 10);

// Does the bus switch (depends on enclstre or kc705 boards), get access to UID chip (to know how to program PLL)
  if (init_softness < 4) {   // en_lines    [This assumes it is a n enclustre]   missing write (0x0, [8*0x0])
      std::vector<uint32_t> v1 = { 0x01, 0x7f };
      uid_I2C.write(0x21, v1, true);    // [0x01, 0x7f], True)  
      std::vector<uint32_t> v2 = { 0x01 };
      uid_I2C.write(0x21, v2, false);   // [0x01], False)
      std::vector<uint32_t> res = uid_I2C.read(0x21, 1);
#ifdef VERBOSE
      std::cout << "I2c enable lines: " << res[0] << std::endl;
#else
      if (res[0]) { /* Defeat compiler unused variable warning */ }
#endif
  }

  uint64_t serial_no = 0;
  if (init_softness < 4) {   // promread
      std::vector<uint32_t> v3 = { 0xfa };
      uid_I2C.write(0x53, v3, false );  // [0xfa], False)
      std::vector<uint32_t> res = uid_I2C.read(0x53, 6);
      for (auto i : res) {
        serial_no = (serial_no << 8) | (i & 0xff);   // Build up the serial_no by sticking 8 more bits on the low end
      }
#ifdef VERBOSE
      std::cout <<  "Unique ID PROM: ";
      for (auto i : res) { 
        std::cout << std::hex << " 0x" << i << std::dec;  // [hex(no) for no in res]
      }
      std::cout << std::endl;
#endif
  }

  if (init_softness < 1) {   // wr_clk_list
      I2CCore clock_I2C = I2CCore(hw, 10, 5, "io.pll_i2c", 0);
      si5344 zeClock = si5344(clock_I2C);
      std::vector<uint32_t> res2 = zeClock.getDeviceVersion();
      zeClock.setPage(0, true);
      zeClock.getPage();
      std::vector<std::pair<uint32_t,uint32_t>> regCfgList = zeClock.parse_clk(serial_no);
      zeClock.writeConfiguration(regCfgList);
  }

  if (init_softness < 4) {   // freq_meas
      for (int i=0;i<2;i++) {
        hw.getNode("io.freq.ctrl.chan_sel").write(i);
        hw.getNode("io.freq.ctrl.en_crap_mode").write(0);
        hw.dispatch();
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        uhal::ValWord<uint32_t> fq = hw.getNode("io.freq.freq.count").read();
        uhal::ValWord<uint32_t> fv = hw.getNode("io.freq.freq.valid").read();
        hw.dispatch();
#ifdef VERBOSE
        std::cout << "Freq: " << i << " " << int(fv) << " " << int(fq) * 119.20928 / 1000000 << std::endl;
#else
        dune::DAQLogger::LogInfo(instancename) << "Timing module Freq: " << i << " " 
                                               << int(fv) << " " << int(fq) * 119.20928 / 1000000;
#endif
      }
  }

  if (init_softness < 4) {   // sfp_tx_dis        
      hw.getNode("io.csr.ctrl.sfp_tx_dis").write(0);
      hw.dispatch();
  }

  if (init_softness < 1) {   // ctrl.rst.   Clock is going, now have to reset everything in 50MHz domain (destructive)
      hw.getNode("io.csr.ctrl.rst").write(1);
      hw.dispatch();
      hw.getNode("io.csr.ctrl.rst").write(0);
      hw.dispatch();
  }

  if (init_softness != 0) {   // 
      dune::DAQLogger::LogWarning(instancename) << "Timing: Some initialiation bypassed as requested " 
                                                << "by config settings. init_softness = " << init_softness << "\n";
  }
}

// The VERBOSE switch is not used here as it is controlled from the caller TimingReceiver_generator.cc
void TimingSequence::hwstatus(uhal::HwInterface& hw) {
    std::cout << "hw.id() = " << hw.id() << std::endl;
    uhal::ValWord<uint32_t> reg = hw.getNode("io.csr.stat").read();
    hw.dispatch();
    std::cout << "reg = 0x" << std::hex << reg << std::dec << std::endl;
}

