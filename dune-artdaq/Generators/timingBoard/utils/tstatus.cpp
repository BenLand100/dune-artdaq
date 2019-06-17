//#!/usr/bin/python

//# -*- coding: utf-8 -*-

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

#include "uhal/uhal.hpp"

int main() {
  uhal::setLogLevelTo(uhal::Notice());
  uhal::ConnectionManager manager ("file://connections.xml"); 

  uhal::HwInterface hw_rx = manager.getDevice("DUNE_FMC_RX");

  std::cout << "hw.id() = " << hw_rx.id() << std::endl;
  uhal::ValWord<uint32_t> reg = hw_rx.getNode("io.csr.stat").read();
  hw_rx.dispatch();
  std::cout << "reg = 0x" << std::hex << reg << std::dec << std::endl; 
  return 0;
}


