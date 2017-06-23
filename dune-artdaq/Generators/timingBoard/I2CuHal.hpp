#ifndef I2CUHAL_HPP
#define I2CUHAL_HPP

// This include file is in /opt/cactus/include/uhal/uhal.hpp
#include "uhal/uhal.hpp"
#include <string>
#include <vector>

//################################################################################
//# /*
//#        I2C CORE
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//# */
//################################################################################

class I2CCore {
//    """I2C communication block."""

private:
//    # Define bits in cmd_stat register.  
//    In Python these were used as e.g. I2CCore.recvdack (i.e. change . to _)
    const uint32_t I2CCore_startcmd   = 0x1 << 7;
    const uint32_t I2CCore_stopcmd    = 0x1 << 6;
    const uint32_t I2CCore_readcmd    = 0x1 << 5;
    const uint32_t I2CCore_writecmd   = 0x1 << 4;
    const uint32_t I2CCore_ack        = 0x1 << 3;
    const uint32_t I2CCore_intack     = 0x1;

    const uint32_t I2CCore_recvdack   = 0x1 << 7;
    const uint32_t I2CCore_busy       = 0x1 << 6;
    const uint32_t I2CCore_arblost    = 0x1 << 5;
    const uint32_t I2CCore_inprogress = 0x1 << 1;
    const uint32_t I2CCore_interrupt  = 0x1;

    // In the translation, e.g. self.ctrl from python is changed to crtl_
    uhal::HwInterface& target_;   // This is usually what is called 'hw' in examples
    const std::string name_;
    const uint32_t delay_;
    const uhal::Node& prescale_low_;
    const uhal::Node& prescale_high_;
    const uhal::Node& ctrl_;
    const uhal::Node& data_;
    const uhal::Node& cmd_stat_;
    const uint32_t wishboneclock_;
    const uint32_t i2cclock_;

    // Methods

    void bs(uint32_t n, uint32_t val, char*retval);  // Private, used internally by printing in state()

public:
    I2CCore(uhal::HwInterface& target, uint32_t wclk, uint32_t i2cclk, std::string name, uint32_t delay=0);
    void state();
    void clearint();
    void config();
    bool checkack();
    bool delayorcheckack();
    int32_t write(uint32_t addr, std::vector<uint32_t> data, bool stop=true);
    std::vector<uint32_t> read(uint32_t addr, uint32_t n);
};

#endif
