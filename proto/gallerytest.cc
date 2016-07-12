
// JCF, Jul-12-2016

// This is a very simple standalone executable designed to test the
// gallery package, needed for artdaq v1_13_00 (and higher)-based
// versions of lbne-artdaq upgrade due to the use of the Playback
// generator. It's heavily influenced by Marc Paterno's example
// program presented in the June 17 Art Users meeting at Fermilab. If
// you wish to use this yourself, you'll want to manually set the
// relevant filenames / input tags to reflect what your input is. 

#include "artdaq-core/Data/Fragments.hh"

#include "canvas/Utilities/InputTag.h"
#include "canvas/Persistency/Common/FindMany.h"
#include "gallery/Event.h"
#include "gallery/ValidHandle.h"

#include <string>
#include <vector>
#include <iostream>

int main() {
  
  std::vector<std::string> filenames = { 
    "/data/lbnedaq/scratch/jcfree/input_datafiles_for_playback/lbne_r000101_sr01_20160603T161541.root" 
  };

  const std::string tag = "daq:TOY1:DAQ";

  size_t cntr = 1;

  for (gallery::Event ev(filenames); !ev.atEnd(); ev.next()) {
    const auto& fragments = *ev.getValidHandle<artdaq::Fragments>(tag);

    for (const auto& frag : fragments) {
      std::cout << "Frag #" << cntr++ << ", seqID == " << frag.sequenceID() << ", fragID == " <<
	frag.fragmentID() << ", payload size " << frag.dataSizeBytes() << std::endl;
    }
  }  

  return 0;
}
