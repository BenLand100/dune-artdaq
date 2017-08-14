#define VERBOSE

#include "I2CuHal.hpp"
#include <chrono>    // For std::chrono::milliseconds
#include <thread>    // For std::thread::sleep_for

//################################################################################
//# /*
//#        I2C CORE
//#
//#  Translated from Python by Giles Barr May 2017 for ProtoDUNE
//#  The pythin version was created by the Bristol High Energy Physics group
//# */
//################################################################################

// Constructor
I2CCore::I2CCore(uhal::HwInterface& target, uint32_t wclk, uint32_t i2cclk, std::string name, uint32_t delay)
  : target_(target), 
    name_(name), 
    delay_(delay), 
    prescale_low_(target_.getNode(name + ".ps_lo")),
    prescale_high_(target_.getNode(name + ".ps_hi")),
    ctrl_(target_.getNode(name + ".ctrl")),
    data_(target_.getNode(name + ".data")),
    cmd_stat_(target_.getNode(name + ".cmd_stat")),
    wishboneclock_(wclk), 
    i2cclock_(i2cclk) 
{  
  this->config();
}

// Routine to print a binary formatted number.  Unfortunately 
// C++ doesn't have a function for that while Python does.
// This function used in state() below.
// C++ doesn't have a binary print.  You must pass this routine a char[33] in retval.
void I2CCore::bs(uint32_t n, uint32_t val, char*retval) {
  if (n>32) n=32;   // Don't do more than 32 bits
  //if (n<0) n=0;     // Bail essentially
  for (unsigned int i=0;i<n;i++) {
    retval[i] = "01"[ (val&(0x1<<(n-i-1))) ];   
    // This is horribly obtuse, but really cute.  
    // Well at least it amuses Giles!  Inside the [] we get 0 or 1, 
    // and this selects the 0th or 1th character from "01"!
    // (n-i-1) stops it being backwards: e.g. n=8, 76543210 -> 01234567
  }
  retval[n] = '\0';
  return;
}

void I2CCore::state() {
  uhal::ValWord<uint32_t> s_ps_low   = prescale_low_.read();
  uhal::ValWord<uint32_t> s_ps_high  = prescale_high_.read();
  uhal::ValWord<uint32_t> s_ctrl     = ctrl_.read();
  uhal::ValWord<uint32_t> s_data     = data_.read();
  uhal::ValWord<uint32_t> s_cmd_stat = cmd_stat_.read();
  target_.dispatch();
  
#ifdef VERBOSE
  uint32_t s_prescale = (s_ps_high << 8) | s_ps_low;
  char fmt[] = "\treg %8s = %10d 0x%8.8x %16s\n";
  char r[33];  // For the binary printing result
  uint32_t s;  // You can't pass a ValWord<> to printf, grab the value first
  
  s = s_ps_low;   this->bs( 8,s,r);  printf(fmt,"ps_low"  ,s,s,r);
  s = s_ps_high;  this->bs( 8,s,r);  printf(fmt,"ps_high" ,s,s,r);
  s = s_ctrl;     this->bs(16,s,r);  printf(fmt,"ctrl"    ,s,s,r);
  s = s_data;     this->bs(16,s,r);  printf(fmt,"data"    ,s,s,r);
  s = s_cmd_stat; this->bs(16,s,r);  printf(fmt,"cmd_stat",s,s,r);
  s = s_prescale; this->bs(16,s,r);  printf(fmt,"prescale",s,s,r);
#endif
}

void I2CCore::clearint() {
  ctrl_.write(0x1);
  target_.dispatch();
}

void I2CCore::config() {    // INITIALIZATION OF THE I2S MASTER CORE
  ctrl_         .write(0x0 << 7);                 // Disable core
  target_.dispatch();
  
  // #Write pre-scale register
  // #prescale = int(self.wishboneclock / (5.0 * self.i2cclock)) - 1
  uint32_t prescale = 0x0100;    // #FOR NOW HARDWIRED, TO BE MODIFIED
  // uint32_t prescale = 0x2710; // #FOR NOW HARDWIRED, TO BE MODIFIED (this line commented in python)
  
  prescale_low_ .write(prescale & 0xff);
  prescale_high_.write((prescale & 0xff00) >> 8);
  ctrl_         .write(0x1 << 7);                 // Enable core
  target_.dispatch();
}

bool I2CCore::checkack() {
  bool inprogress = true;
  bool ack = false;
  //printf("c1.1\n");
  int32_t nloop = 0;
  while (inprogress) {
    //uhal::ValWord<uint32_t> s_cmd_stat = cmd_stat_.read();
    uhal::ValWord<uint32_t> s_cmd_stat = target_.getNode(name_ + ".cmd_stat").read();
    target_.dispatch();
    uint32_t u_cmd_stat = s_cmd_stat;
    inprogress = ( ((u_cmd_stat & I2CCore_inprogress) > 0) );
    ack = ( (s_cmd_stat & I2CCore_recvdack) == 0);
    //if (nloop<10) printf("cmd_stat.read(): path %s readval %4.4x maskinprog %4.4x maskack %4.4x inprog %4.4x ack %4.4x\n"
    //           ,cmd_stat_.getPath().c_str(),u_cmd_stat,I2CCore_inprogress,I2CCore_recvdack,inprogress,ack);
    nloop++;
  }
  //printf("c1.3 exit %d\n",nloop);
  return ack;
}

bool I2CCore::delayorcheckack() {
  bool ack = true;
  if (delay_ == 0) {
    ack = this->checkack();
  } else {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_)); 
    // (Uses headers <chrono> and <thread>)
    ack = this->checkack();      // #Remove this?
  }
  return ack;
}

//################################################################################
//# /*
//#        I2C WRITE
//# */
//################################################################################

int32_t I2CCore::write(uint32_t addr, std::vector<uint32_t> data, bool stop) {
  // """Write data to the device with the given address."""
  // # Start transfer with 7 bit address and write bit (0)
  int32_t nwritten = -1;
  addr &= 0x7f;
  addr = addr << 1;
  // uint32_t startcmd = 0x1 << 7;   // These duplicate I2CCore_startcmd etc in class. 
  // uint32_t stopcmd  = 0x1 << 6;   // so I am omitting these local ones in the C++
  // uint32_t writecmd = 0x1 << 4;   // translation.

  std::string tmp1 = data_.getPath();
  // printf("data_.write(addr): add 0x%x path %s val 0x%x\n",data_.getAddress(),data_.getPath().c_str(),addr);

  // #Set transmit register (write operation, LSB=0)
  target_.getNode(name_ + ".data").write(addr);
  //data_.write(addr);
  // #Set Command Register to 0x90 (write, start)
  // printf("cmd_stat_.write(I2CCore_startcmd | I2CCore_writecmd): add 0x%x path %s val 0x%x\n",cmd_stat_.getAddress(),cmd_stat_.getPath().c_str(),(I2CCore_startcmd | I2CCore_writecmd));
  target_.getNode(name_ + ".cmd_stat").write(I2CCore_startcmd | I2CCore_writecmd);
  //cmd_stat_.write(I2CCore_startcmd | I2CCore_writecmd);
  target_.dispatch();
  //printf("i1.2\n");
  bool ack = this->delayorcheckack();
  //printf("i1.3\n");  

  if (!ack) {
        //printf("i1.4\n");
    cmd_stat_.write(I2CCore_stopcmd);
    target_.dispatch();
    return nwritten;
  }
  nwritten += 1;
  
  for (auto val : data) {
      //printf("i1.5\n");
    val &= 0xff;
    // #Write slave memory address
    data_.write(val);
  //printf("i1.6 %4.4x\n",I2CCore_writecmd);
    // #Set Command Register to 0x10 (write)
    cmd_stat_.write(I2CCore_writecmd);
    target_.dispatch();
  //printf("i1.7\n");
    bool ack = this->delayorcheckack();
    if (!ack) {
  //printf("i1.8\n");
      cmd_stat_.write(I2CCore_stopcmd);
      target_.dispatch();
      return nwritten;
    }
  //printf("i1.9\n");
    nwritten += 1;
  }

  //printf("i1.10\n");  
  if (stop) {
      //printf("i1.11\n");
    cmd_stat_.write(I2CCore_stopcmd);
  //printf("i1.12\n");
    target_.dispatch();
  }
  
  return nwritten;
}

// ################################################################################
// # /*
// #        I2C READ
// # */
// ################################################################################
std::vector<uint32_t> I2CCore::read(uint32_t addr, uint32_t n) {
  // """Read n bytes of data from the device with the given address."""
  // # Start transfer with 7 bit address and read bit (1)
  std::vector<uint32_t> datavector;   // In Python this is 'data', but that is close to 'data_'
  addr &= 0x7f;
  addr = addr << 1;
  addr |= 0x1; // # read bit
  data_.write(addr);
  cmd_stat_.write(I2CCore_startcmd | I2CCore_writecmd);
  target_.dispatch();
  
  bool ack = this->delayorcheckack();
  if (!ack) {
    cmd_stat_.write(I2CCore_stopcmd);
    target_.dispatch();
    return datavector;
  }
  
  for (unsigned int i=0; i<n; i++) {    // for i in range(n) i.e. 0...(n-1)
    if (i < (n-1)) {
      cmd_stat_.write(I2CCore_readcmd); // # <---
    } else {
      cmd_stat_.write(I2CCore_readcmd | I2CCore_ack | I2CCore_stopcmd);
      // # <--- This tells the slave that it is the last word
      target_.dispatch();
      // NOTE FROM GILES.  THIS IS EXACTLY HOW IT IS DONE IN PYTHON, But it dosn't
      // really make sense, the dispatch() above should either be per data word,
      // or the ack below should be done outside the loop I think.  It is fine for 1 word.
      // Or maybe the ack should be inside this else?  Note nobody uses the value of ack.
    }
    /* bool ack =*/ this->delayorcheckack();
    uhal::ValWord<uint32_t> val = data_.read();
    target_.dispatch();
    datavector.push_back(val & 0xff);
  }
  // #self.cmd_stat.write(I2CCore.stopcmd)
  // #self.target.dispatch()
  return datavector;
}

// writeread() is completely commented out in the python, so we have not converted it.
// It looks straightforward except that it returns a python tuple.
// ################################################################################
// # /*
// #        I2C WRITE-READ
// # */
// ################################################################################

// # def writeread(self, addr, data, n):
// #     """Write data to device, then read n bytes back from it."""
// #     nwritten = self.write(addr, data, stop=False)
// #     readdata = []
// #     if nwritten == len(data):
// #         readdata = self.read(addr, n)
// #     return nwritten, readdata

//---------------------------------------------------------------------------------------------
// The following SPI core code was in the original python file, but is not needed for ProtoDUNE
//---------------------------------------------------------------------------------------------

#if 0
//"""
SPI core XML:

<node description="SPI master controller" fwinfo="endpoint;width=3">
    <node id="d0" address="0x0" description="Data reg 0"/>
    <node id="d1" address="0x1" description="Data reg 1"/>
    <node id="d2" address="0x2" description="Data reg 2"/>
    <node id="d3" address="0x3" description="Data reg 3"/>
    <node id="ctrl" address="0x4" description="Control reg"/>
    <node id="divider" address="0x5" description="Clock divider reg"/>
    <node id="ss" address="0x6" description="Slave select reg"/>
</node>
//""" 
class SPICore:

    go_busy = 0x1 << 8
    rising = 1
    falling = 0


    def __init__(self, target, wclk, spiclk, basename="io.spi"):
        self.target = target
        // # Only a single data register is required since all transfers are
        // # 16 bit long
        self.data = target.getNode("%s.d0" % basename)
        self.control = target.getNode("%s.ctrl" % basename)
        self.control_val = 0b0
        self.divider = target.getNode("%s.divider" % basename)
        self.slaveselect = target.getNode("%s.ss" % basename)
        self.divider_val = int(wclk / spiclk / 2.0 - 1.0)
        self.divider_val = 0x7f
        self.configured = False

    def config(self):
        "Configure SPI interace for communicating with ADCs."
        self.divider_val = int(self.divider_val) % 0xffff
        if verbose:
            print "Configuring SPI core, divider = 0x%x" % self.divider_val
        self.divider.write(self.divider_val)
        self.target.dispatch()
        self.control_val = 0x0
        self.control_val |= 0x0 << 13 // # Automatic slave select
        self.control_val |= 0x0 << 12 // # No interrupt
        self.control_val |= 0x0 << 11 // # MSB first
        // # ADC samples data on rising edge of SCK
        self.control_val |= 0x1 << 10 // # change ouput on falling edge of SCK
        // # ADC changes output shortly after falling edge of SCK
        self.control_val |= 0x0 << 9 // # read input on rising edge
        self.control_val |= 0x10 // # 16 bit transfers
        if verbose:
            print "SPI control val = 0x%x = %s" % (self.control_val, bin(self.control_val))
        self.configured = True

    def transmit(self, chip, value):
        if not self.configured:
            self.config()
        assert chip >= 0 and chip < 8
        value &= 0xffff
        self.data.write(value)
        checkdata = self.data.read()
        self.target.dispatch()
        assert checkdata == value
        self.control.write(self.control_val)
        self.slaveselect.write(0xff ^ (0x1 << chip))
        self.target.dispatch()
        self.control.write(self.control_val | SPICore.go_busy)
        self.target.dispatch()
        busy = True
        while busy:
            status = self.control.read()
            self.target.dispatch()
            busy = status & SPICore.go_busy > 0
        self.slaveselect.write(0xff)
        data = self.data.read()
        ss = self.slaveselect.read()
        status = self.control.read()
        self.target.dispatch()
        // #print "Received data: 0x%x, status = 0x%x, ss = 0x%x" % (data, status, ss)
        return data
#endif
