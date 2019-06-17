#include "dune-raw-data/Overlays/NanoSlice.hh"

dune::NanoSlice::NanoSlice(uint8_t* address) : buffer_(address) { }

dune::NanoSlice::Header::nanoslice_size_t dune::NanoSlice::size() const
{
  return header_()->nanoslice_size;
}

dune::NanoSlice::Header::channel_number_t dune::NanoSlice::channelNumber() const
{
  return header_()->channel_number;
}

dune::NanoSlice::Header::sample_count_t dune::NanoSlice::sampleCount() const
{
  return header_()->sample_count;
}

bool dune::NanoSlice::sampleValue(uint32_t index, uint16_t& value) const
{
  if (index >= sampleCount()) {return false;}
  value = data_()[index];
  return true;
}

dune::NanoSlice::Header const* dune::NanoSlice::header_() const
{
  return reinterpret_cast<Header const*>(buffer_);
}

uint16_t const* dune::NanoSlice::data_() const
{
  return reinterpret_cast<uint16_t const*>(buffer_ + sizeof(Header));
}
