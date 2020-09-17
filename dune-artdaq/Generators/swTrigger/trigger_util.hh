#include "dune-raw-data/Overlays/FragmentType.hh"
#include "dune-raw-data/Overlays/TimingFragment.hh"

namespace trigger_util
{
    //-----------------------------------------------------------------------
    std::unique_ptr<artdaq::Fragment> makeTriggeringFragment(uint64_t timestamp,
                                                             uint64_t sequence_id,
                                                             uint16_t fragment_id)
    {
        std::unique_ptr<artdaq::Fragment> f = artdaq::Fragment::FragmentBytes( dune::TimingFragment::size() * sizeof(uint32_t),
                                                                               artdaq::Fragment::InvalidSequenceID,
                                                                               artdaq::Fragment::InvalidFragmentID,
                                                                               artdaq::Fragment::InvalidFragmentType,
                                                                               dune::TimingFragment::Metadata(dune::TimingFragment::VERSION));
        // It's unclear to me whether the constructor above actually sets the metadata, so let's do it here too to be sure
        f->updateMetadata(dune::TimingFragment::Metadata(dune::TimingFragment::VERSION));
        f->setSequenceID( sequence_id );
        f->setFragmentID( fragment_id );
        f->setUserType( dune::detail::TIMING );
        f->setTimestamp(timestamp);  // 64-bit number

        // The real fragment from the timing board reader has 6 32-bit
        // words from the actual timing board, plus 6 more 32-bit words
        // for some run- and spill-related timestamps used for beam
        // matching. We'll just zero them all out except the timestamp, for now
        uint32_t* word = reinterpret_cast<uint32_t*>(f->dataBeginBytes());
        for(size_t i=0; i<dune::TimingFragment::size(); ++i){
            word[i]=0;
        }
        // Word 2 is the low 32 bits of the timestamp
        word[2]=timestamp & 0xffffffff;
        // Word 3 is the high 32 bits of the timestamp
        word[3]=(timestamp >> 32) & 0xffffffff;

        return f;
    }


}
