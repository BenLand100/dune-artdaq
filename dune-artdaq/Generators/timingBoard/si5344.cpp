#include "I2CuHal.hpp"
#include "si5344.hpp"

#include "messagefacility/MessageLogger/MessageLogger.h"  // For log messages
#include "dune-artdaq/DAQLogger/DAQLogger.hh"   // For log messsages

// Select one of the two lines below.  The first turns printing on, 
// like in the python scripts, the second turns printing off.
//#define SVERBOSE(a)
#define SVERBOSE(a) a

//################################################################################
//# /*
//#        si5344 i2c manipulation functions
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//# */
//################################################################################

std::vector<uint32_t> si5344::readRegister(uint32_t myaddr, uint32_t nwords) {
  //### Read a specific register on the Si5344 chip. There is 
  //### not check on the validity of the address but
  //### the code sets the correct page before reading.
  
  //# First make sure we are on the correct page
  std::vector<uint32_t> currentPg = this->getPage();  // Returns 1 value
  uint32_t requirePg = (myaddr & 0xFF00) >> 8;
  //#  print "REG", hex(myaddr), "CURR PG" , hex(currentPg[0]), "REQ PG", hex(requirePg)
  if (currentPg[0] != requirePg) {
    this->setPage(requirePg);
  }
  //#Now read from register.
  std::vector<uint32_t> addr;
  addr.push_back(myaddr);
  bool mystop = false;
  i2c_.write(slaveaddr_, addr, mystop);
  //# time.sleep(0.1)
  std::vector<uint32_t> res = i2c_.read(slaveaddr_, nwords);
  return res;
}

void si5344::writeRegister(uint32_t myaddr, std::vector<uint32_t> data, uint32_t verbose) {
  //### Write a specific register on the Si5344 chip. There 
  //### is not check on the validity of the address but
  //### the code sets the correct page before reading.
  //### myaddr is an int
  //### data is a list of ints
  
  //#First make sure we are on the correct page
  myaddr = myaddr & 0xFFFF;
  std::vector<uint32_t> currentPg = this->getPage();
  uint32_t requirePg = (myaddr & 0xFF00) >> 8;
  //# print "REG", hex(myaddr), "CURR PG" , currentPg, "REQ PG", hex(requirePg)
  if (currentPg[0] != requirePg) {
    this->setPage(requirePg);
  }
  //#Now write to register.
  // data.insert(0, myaddr);
  std::vector<uint32_t> addrdata;
  addrdata.push_back(myaddr);
  for (auto idata : data) addrdata.push_back(idata);  
  // addrdata is now myaddr followed by the elements of data

  SVERBOSE( verbose = 0; )   // Can select at the top whether to emulate printing 
                             // from python scripts or suppress it for production run
  if (verbose) {
    printf("  Writing: \n\t  ");
    for (auto iaddr : addrdata) { printf("%2.2x ",iaddr); }
    printf("\n");
  }
  i2c_.write(slaveaddr_, addrdata);
  //#time.sleep(0.01)
}

void si5344::setPage(uint32_t page, uint32_t verbose) {
  //#Configure the chip to perform operations on the specified address page.
  bool mystop = true;
  std::vector<uint32_t> myaddr = {0x01, page};
  i2c_.write(slaveaddr_, myaddr, mystop);
  //#time.sleep(0.01)

  SVERBOSE( verbose = 0; )   // Can select at the top whether to emulate printing 
                             // from python scripts or suppress it for production run
  if (verbose) { 
    printf("Si5344 Set Reg Page: %d\n", page); 
  }
}

std::vector<uint32_t> si5344::getPage(uint32_t verbose) {
  //#Read the current address page
  uint32_t mystop=false;
  std::vector<uint32_t> myaddr = { 0x01 };
  i2c_.write(slaveaddr_, myaddr, mystop);
  std::vector<uint32_t> rPage = i2c_.read(slaveaddr_, 1);
  //#time.sleep(0.1)

  SVERBOSE( verbose = 0; )   // Can select at the top whether to emulate printing 
                             // from python scripts or suppress it for production run
  if (verbose) {
    printf("  Page read: %d", rPage[0]);
  }
  return rPage;
}

std::vector<uint32_t> si5344::getDeviceVersion() {
  //#Read registers containing chip information
  uint32_t mystop = false;
  uint32_t nwords = 2;
  std::vector<uint32_t> myaddr = { 0x02 };     //#0xfa
  this->setPage(0);
  i2c_.write(slaveaddr_, myaddr, mystop);
  //#time.sleep(0.1)
  std::vector<uint32_t> res = i2c_.read(slaveaddr_, nwords);

  uint32_t verbose = 1;

  SVERBOSE( verbose = 0; )   // Can select at the top whether to emulate printing 
                             // from python scripts or suppress it for production run
  if (verbose) {
    printf("  CLOCK EPROM: \t  ");
    for (auto iaddr : res) printf("%2.2x ",iaddr);
    printf("\n");
  }

  return res;
}

// This is the old routine, it loads file 3 but ignores the filename
#if 0
std::vector<std::pair<uint32_t,uint32_t>> si5344::parse_clk(std::string /*filename*/) {
  // Temporary code
  return { 
#include "addrtab/PDTS0003.hh"
 };
}
#endif

// This is the new routine, the parameter is a vector of 6 bytes 
// containing the serial number of the board.  This then calls either 
// parse_clk0() or parse_clk3().  The default is PDTS0003.hh
std::vector<std::pair<uint32_t,uint32_t>> si5344::parse_clk(uint64_t serialno) {
  int clk = 3;
  int defaulted = 1;

  // These next few are examples, we must add a table of correct serial numbers
  if (serialno == 0x0000112233445566) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000112233445566) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e2b2e) { clk = 3; defaulted = 0; } // The one in the pizza box at CERN 

  if (serialno == 0x0000d880395e720b) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e501a) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e50b8) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e501b) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e7201) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e4fcc) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e5069) { clk = 0; defaulted = 0; }
  if (serialno == 0x0000d880395e7206) { clk = 0; defaulted = 0; }

  if (serialno == 0x0000d880395e1c86) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e2630) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e262b) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e2b38) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e1a6a) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e36ae) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e2b2e) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e2b33) { clk = 3; defaulted = 0; }
  if (serialno == 0x0000d880395e1c81) { clk = 3; defaulted = 0; }

  dune::DAQLogger::LogInfo(instancename_) 
            << "Setting up si5344 on timing board serial number 0x" << std::hex 
            << serialno << " using PDTS000" << std::dec << clk 
            << ((defaulted)? " s/n unknown, defaulted" : " selected from s/n") 
            << std::endl;
  if (clk == 3) {
    return this->parse_clk3();
  } else {
    return this->parse_clk0();
  }
}

std::vector<std::pair<uint32_t,uint32_t>> si5344::parse_clk0() {
  // Temporary code
  return {
#include "addrtab/PDTS0000.hh"
 };
}

std::vector<std::pair<uint32_t,uint32_t>> si5344::parse_clk3() {
  // Temporary code
  return {
#include "addrtab/PDTS0003.hh"
 };
}

#if 0
    def parse_clk(self, filename):
        //#Parse the configuration file produced by Clockbuilder Pro (Silicon Labs)
    	deletedcomments=""""""
    	print "\tParsing file", filename
    	with open(filename, 'rb') as configfile:
    		for i, line in enumerate(configfile):
    		    if not line.startswith('#') :
    		      if not line.startswith('Address'):
    			deletedcomments+=line
    	csvfile = StringIO.StringIO(deletedcomments)
    	cvr= csv.reader(csvfile, delimiter=',', quotechar='|')
    	//#print "\tN elements  parser:", sum(1 for row in cvr)
    	//#return [addr_list,data_list]
        //# for item in cvr:
        //#     print "rere"
        //#     regAddr= int(item[0], 16)
        //#     regData[0]= int(item[1], 16)
        //#     print "\t  ", hex(regAddr), hex(regData[0])
        //#self.configList= cvr
        regSettingList= list(cvr)
        print "\t  ", len(regSettingList), "elements"

        return regSettingList
#endif

void si5344::writeConfiguration(std::vector<std::pair<uint32_t,uint32_t>>regSettingList) {
  printf("\tWrite configuration:\n");
  uint32_t counter=0;
  for (auto ad : regSettingList) {
    uint32_t regAddr = ad.first;
    std::vector<uint32_t> regData;
    regData.push_back(ad.second);
    counter++;
    this->writeRegister(regAddr, regData);
  }
}

