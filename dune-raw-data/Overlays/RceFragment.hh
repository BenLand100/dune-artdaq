#ifndef artdaq_dune_Overlays_RceFragment_hh
#define artdaq_dune_Overlays_RceFragment_hh

#include <memory>
#include <string>
#include <ostream>
#include "dam/DataFragmentUnpack.hh"
#include "dam/TpcFragmentUnpack.hh"
#include "artdaq-core/Data/Fragment.hh"

namespace dune
{
    class RceFragment;
    typedef std::vector<dune::RceFragment> RceFragments;
}

class TpcStreamUnpack;

class dune::RceFragment
{
    public:

        RceFragment(artdaq::Fragment const & fragment );
        RceFragment(const uint64_t* data_ptr);
        int size() const { return _n_streams; }
        TpcStreamUnpack const * get_stream(int i) const;
        void hexdump(std::ostream& out, int n_words=10) const;
        void save(const std::string& filepath) const;

        static RceFragments from_container_frags(const artdaq::Fragments& frags);

    private:
        void _init();

        std::unique_ptr<DataFragmentUnpack> _data_fragment;
        std::unique_ptr<TpcFragmentUnpack> _tpc_fragment;
        int _n_streams = 0;
        const uint64_t* _data_ptr;
};
#endif 
