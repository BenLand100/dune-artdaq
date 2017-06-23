#ifndef SI5344_HPP
#define SI5344_HPP

//################################################################################
//# /*
//#        Class to configure the si5344 clock generator over i2c
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//# */
//################################################################################

class I2CCore;  // Forward definition

#define VERBOSE_DEFAULT 0

class si5344 {
    //#Class to configure the Si5344 clock generator
private:
    I2CCore& i2c_;
    uint32_t slaveaddr_;


public:
    si5344(I2CCore& i2c, uint32_t slaveaddr=0x68) :     // Constructor
       i2c_(i2c), slaveaddr_(slaveaddr) {}

    std::vector<uint32_t> readRegister(uint32_t myaddr, uint32_t nwords);
    void writeRegister(uint32_t myaddr, std::vector<uint32_t> data, uint32_t verbose = VERBOSE_DEFAULT);
    void setPage(uint32_t page, uint32_t verbose=false);
    std::vector<uint32_t> getPage(uint32_t verbose=false);
    std::vector<uint32_t> getDeviceVersion();
    std::vector<std::pair<uint32_t,uint32_t>> parse_clk(std::string filename);
    void writeConfiguration(std::vector<std::pair<uint32_t,uint32_t>>regSettingList);
};
#endif
