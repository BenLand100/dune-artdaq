#ifndef dune_artdaq_Overlays_TimingFragment_hh
#define dune_artdaq_Overlays_TimingFragment_hh

#include "artdaq-core/Data/Fragment.hh"
//#include "artdaq-core/Data/features.hh"
#include "cetlib/exception.h"

#include <ostream>
#include <vector>

// Implementation of "TimingFragment".
//
// From Dave Newbold mail May 2017
// The data words (uint32_t) given by the hardware are
//    d(0) <= X"aa000600";          -- DAQ word 0 (cookie)
//    d(1) <= X"0000000" & scmd;    -- DAQ word 1 (scmd = trigger type, tcmd = the reserved zero bits)
//    d(2) <= tstamp(31 downto 0);  -- DAQ word 2 (tstampl = low bits of timestamp)
//    d(3) <= tstamp(63 downto 32); -- DAQ word 3 (tstamph = high bits of time stamp)
//    d(4) <= evtctr;               -- DAQ word 4 (evtctr = event counter)
//    d(5) <= X"000000000";         -- Dummy checksum, not implemented yet (cksum)
//
// These are stored in the fragment exactly as received and are manipulated
// to be readable by the getters in this class.
//

namespace dune {
  class TimingFragment;

  enum class TimingCommand {
    // From https://twiki.cern.ch/twiki/bin/view/CENF/TimingSystemAdvancedOp retrieved on 2018-10-02
    // The 'sync' bus has the following commands at the moment:
    TimeSync   = 0x0,
    Echo       = 0x1,
    SpillStart = 0x2,
    SpillStop  = 0x3,
    RunStart   = 0x4,
    RunStop    = 0x5,
    WibCalib   = 0x6,
    SSPCalib   = 0x7,
    FakeTrig0  = 0x8,
    FakeTrig1  = 0x9,
    FakeTrig2  = 0xa,
    FakeTrig3  = 0xb,
    BeamTrig   = 0xc,
    NoBeamTrig = 0xd,
    ExtFakeTrig   = 0xe
  };

  // Let the "<<" operator dump the TimingFragment's data to stdout
  std::ostream & operator << (std::ostream &, TimingFragment const &);
}

class dune::TimingFragment {
    
  public:

  struct Metadata {
    typedef uint32_t data_t;

    Metadata(data_t v) : fragment_version(v) {}
    // "8 bits ought to be enough for anyone"
    data_t fragment_version : 8;
    // Give a name to the rest of the bits in case we ever want them for something else
    data_t unused : 24;

    static size_t const size_words = 1ul; // Units of Metadata::data_t
  };

  // Constructor.  This class keeps a copy of a pointer to the fragment.
  // Basically that is how it works.
  public: 
  TimingFragment(artdaq::Fragment const & f ) : artdaq_Fragment_(f) 
    {}


  // The following structure is overlayed onto the data in the
  // fragment, starting at the beginning. The spill timestamps are
  // added by the board reader, and don't exist in the object read
  // directly from the timing board.
  //
  // *******************************************************
  // * If you change anything here, update                 *
  // * TimingFragment::VERSION below!                      *
  // *******************************************************
  struct Body {
    uint32_t cookie  : 32;
    uint32_t scmd    :  4;
    uint32_t tcmd    : 28;
    uint32_t tstampl : 32;
    uint32_t tstamph : 32;
    uint32_t evtctr  : 32;
    uint32_t cksum   : 32;
    uint32_t last_runstart_tstampl;
    uint32_t last_runstart_tstamph;
    uint32_t last_spillstart_tstampl;
    uint32_t last_spillstart_tstamph;
    uint32_t last_spillend_tstampl;
    uint32_t last_spillend_tstamph;
    static size_t const size = 12ul;   // Units of header uint32_t
  };
  // Update this version number if you update anything in the class!
  static constexpr uint32_t VERSION = 3;

  // Here are the getters
  uint32_t get_cookie() const  { return body_()->cookie;  }
  uint32_t get_scmd() const    { return body_()->scmd;    }
  uint32_t get_tcmd() const    { return body_()->tcmd;    }
  uint32_t get_tstampl() const { return body_()->tstampl; }
  uint32_t get_tstamph() const { return body_()->tstamph; }
  uint32_t get_evtctr() const  { return body_()->evtctr;  }
  uint32_t get_cksum() const   { return body_()->cksum;   }

  uint64_t get_tstamp() const { 
    return make_tstamp64(body_()->tstampl, body_()->tstamph);
  } 

  uint64_t get_last_runstart_timestamp() const {
    return make_tstamp64(body_()->last_runstart_tstampl,
                         body_()->last_runstart_tstamph);
  }
  uint64_t get_last_spillstart_timestamp() const {
    return make_tstamp64(body_()->last_spillstart_tstampl,
                         body_()->last_spillstart_tstamph);
  }
  uint64_t get_last_spillend_timestamp() const {
    return make_tstamp64(body_()->last_spillend_tstampl,
                         body_()->last_spillend_tstamph);
  }

  static size_t size() { return Body::size; /* body_()->size; */}    
         // This returns the constant size of the block (cureently 6 uint32_ts)

protected:
  // This function allows the getter classes below to be simple.  
  // It is modelled after header_() in ToyFragment.hh
  // header_() in ToyFragment.hh is protected as well.
  Body const * body_() const {
    return reinterpret_cast<TimingFragment::Body const *>( artdaq_Fragment_.dataBeginBytes());
  }

  uint64_t make_tstamp64(uint32_t tstampl, uint32_t tstamph) const {
     uint64_t l = tstampl;
     uint64_t h = tstamph;
     return (l | (h<<32));
  }
  // This asks the compiler to check if we change te format that we didn't forget to cheange the size as well
  // However if we do change the format, we must figure out how to make this class backward compatible.
  static_assert (sizeof (Body) == Body::size * sizeof (uint32_t), "TimingFragment::Data block size changed");

private:
  artdaq::Fragment const & artdaq_Fragment_;
};

#endif /* dune_artdaq_Overlays_TimingFragment_hh */
