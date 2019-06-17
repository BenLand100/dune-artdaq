#include "dune-raw-data/Overlays/RceFragment.hh"

#include "dam/HeaderFragmentUnpack.hh"
#include "dam/TpcStreamUnpack.hh"

#include "artdaq-core/Data/ContainerFragment.hh"

#include <iostream>
#include <fstream>

dune::RceFragment::RceFragment(const uint64_t* data_ptr) : 
    _data_ptr(data_ptr)
{
    _init();
}

dune::RceFragment::RceFragment(artdaq::Fragment const& afrag) :
    _data_ptr((uint64_t const*)(afrag.dataBeginBytes() + 12))
{
    _init();
}

void dune::RceFragment::_init()
{
    HeaderFragmentUnpack const header (_data_ptr);
    if ( header.isData() )
    {
        _data_fragment = std::make_unique<DataFragmentUnpack>(_data_ptr);
        if (_data_fragment->isTpcNormal())
        {
            _tpc_fragment = std::make_unique<TpcFragmentUnpack>(*_data_fragment);
            _n_streams = _tpc_fragment->getNStreams();
        }
    }
    else
    {
        // LogWarning
    }
}

TpcStreamUnpack const *
dune::RceFragment::get_stream(int i) const
{
    if (_n_streams <= 0)
        return nullptr;
    return _tpc_fragment->getStream(i);
}

void dune::RceFragment::hexdump(std::ostream& out, int n_words) const
{
    out << std::hex;
    for (int i = 0; i < n_words; ++i)
        out << *(_data_ptr + i) << " ";
    out << std::dec << std::endl;
}

void dune::RceFragment::save(const std::string& filepath) const
{
    // TODO check header
    uint64_t header = *_data_ptr;
    size_t bytes = (header >> 8) & 0xffffff;
    bytes *= sizeof(uint64_t);

    std::ofstream b_stream(filepath.c_str(),
            std::fstream::out | std::fstream::binary);

    char const* data = reinterpret_cast<decltype(data)>(_data_ptr);
    b_stream.write(data, bytes);
    b_stream.close();
}

dune::RceFragments
dune::RceFragment::from_container_frags(const artdaq::Fragments& frags)
{
    RceFragments rces;

    for (auto const& frag: frags)
	{
		artdaq::ContainerFragment cfrag(frag);
		for (size_t ii = 0 ; ii < cfrag.block_count(); ++ii)
		{
			auto* data_ptr = (uint8_t*) (cfrag.dataBegin());
 			data_ptr += cfrag.fragmentIndex(ii);

 			size_t afrag_size = 16;
			artdaq::Fragment afrag;
			afrag.resizeBytes(afrag_size);
			memcpy(afrag.headerAddress(), data_ptr, afrag_size);

			int afrag_header_bytes = afrag.dataBeginBytes() 
                - afrag.headerBeginBytes();

            data_ptr += afrag_header_bytes;
            data_ptr += 12;

            dune::RceFragment rce((uint64_t*) data_ptr);
            rces.push_back(std::move(rce));
        }
    }
    //return std::move(rces);
    return rces;
}
