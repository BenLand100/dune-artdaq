#include "dune-raw-data/Overlays/TpcNanoSlice.hh"
#include <iostream>
#include <iomanip>
#include <stdio.h>

dune::TpcNanoSlice::TpcNanoSlice(uint8_t* address) : buffer_(address) { 
  runMode=0x3;//triggered
  raw_payload_words_compressed=24;
  raw_payload_words_uncompressed = 32;
  num_channels=128;
}

dune::TpcNanoSlice::TpcNanoSlice(uint8_t* address, uint8_t mode) : buffer_(address) {
  runMode=mode;
  if(runMode==0x1){
    //scope mode
    raw_payload_words_compressed = 1;
    raw_payload_words_uncompressed = 1;
    num_channels=1;
  }else if(runMode==0x3||runMode==0x2){
    //triggered mode
    raw_payload_words_compressed=24;
    raw_payload_words_uncompressed = 32;
    num_channels=128;    
  }

 }

dune::TpcNanoSlice::nanoslice_size_t dune::TpcNanoSlice::size() const
{
//	nanoslice_size_t raw_payload_words =
//			this->data_compressed() ? raw_payload_words_compressed : raw_payload_words_uncompressed;
	nanoslice_size_t raw_payload_words = raw_payload_words_uncompressed;
	if(runMode==0x1)// scope mode
	  return (dune::TpcNanoSlice::nanoslice_size_t)((Header::raw_header_words)* sizeof(raw_data_word_t)+sizeof(uint16_t));
	else 
	  return (dune::TpcNanoSlice::nanoslice_size_t)((Header::raw_header_words + raw_payload_words)* sizeof(raw_data_word_t));
}

dune::TpcNanoSlice::Header::format_version_t dune::TpcNanoSlice::format_version() const
{
	uint8_t header_format_version = (header_()->raw_header_data[0] >> 56) & 0xF;
	return static_cast<dune::TpcNanoSlice::Header::format_version_t>(header_format_version);
}

dune::TpcNanoSlice::Header::nova_timestamp_t dune::TpcNanoSlice::nova_timestamp() const
{
	uint64_t nova_timestamp = (header_()->raw_header_data[0] & 0xFFFFFFFFFFFFFF);
	//		std::cout << " TPC::Nanoslice  NOvA timestamp   : 0x" << std::hex << std::setw(14) << std::setfill('0')
	//							      << nova_timestamp << std::dec << std::endl;
	return static_cast<dune::TpcNanoSlice::Header::nova_timestamp_t>(nova_timestamp);
}

// 2/12/2015--mg right now we only have a header word == nova_timestamp...everything below referring to the header[1-x] isn't there...
dune::TpcNanoSlice::Header::rce_counter_t dune::TpcNanoSlice::rce_counter() const
{
	uint32_t rce_counter = (header_()->raw_header_data[1] >> 32) & 0xFFFFFFFF;
	return static_cast<dune::TpcNanoSlice::Header::rce_counter_t>(rce_counter);
}

dune::TpcNanoSlice::Header::frontend_counter_t dune::TpcNanoSlice::frontend_counter() const
{
	uint32_t frontend_counter = (header_()->raw_header_data[1]) & 0xFFFFFFFF;
	return static_cast<dune::TpcNanoSlice::Header::frontend_counter_t>(frontend_counter);
}

// Returns the data packet type from the header
dune::TpcNanoSlice::Header::data_packet_type_t dune::TpcNanoSlice::data_packet_type() const
{
	uint8_t data_packet_type = (header_()->raw_header_data[2] >> 62) & 0xF;
	return static_cast<dune::TpcNanoSlice::Header::data_packet_type_t>(data_packet_type);
}

// Returns the misalignment detected flag from the header
dune::TpcNanoSlice::Header::misalign_detected_t dune::TpcNanoSlice::misalign_detected() const
{
	bool misalign_detected = (header_()->raw_header_data[2] >> 61) & 0x1;
	return static_cast<dune::TpcNanoSlice::Header::misalign_detected_t>(misalign_detected);
}

// Returns to PGP lane Tag ID error flag from the header
dune::TpcNanoSlice::Header::pgp_lane_tag_id_error_t dune::TpcNanoSlice::pgp_lane_tag_id_error() const
{
	bool pgp_lane_tag_id_error = (header_()->raw_header_data[2] >> 60) & 0x1;
	return static_cast<dune::TpcNanoSlice::Header::pgp_lane_tag_id_error_t>(pgp_lane_tag_id_error);
}

// Returns the PGP lane sync error from the header
dune::TpcNanoSlice::Header::pgp_lane_sync_error_t dune::TpcNanoSlice::pgp_lane_sync_error(
		dune::TpcNanoSlice::pgp_lane_id const lane) const
{
	bool pgp_lane_sync_error = (header_()->raw_header_data[2] >> (56 + lane)) & 0x1;
	return static_cast<dune::TpcNanoSlice::Header::pgp_lane_sync_error_t>(pgp_lane_sync_error);
}

// Returns the data compression flag from the header
dune::TpcNanoSlice::Header::data_compressed_t dune::TpcNanoSlice::data_compressed() const
{
	bool data_compressed = (header_()->raw_header_data[2] >> 36) & 0x1;
	return static_cast<dune::TpcNanoSlice::Header::data_compressed_t>(data_compressed);
}

// Returns the PGP lane masked bits from the header
dune::TpcNanoSlice::Header::pgp_lane_masked_t dune::TpcNanoSlice::pgp_lane_masked(
		dune::TpcNanoSlice::pgp_lane_id const lane) const
{
	bool pgp_lane_masked = (header_()->raw_header_data[2] >> (32 + lane)) & 0x1;
	return static_cast<dune::TpcNanoSlice::Header::pgp_lane_masked_t>(pgp_lane_masked);
}

// Returns the round_trip sync delay over PGP from the header
dune::TpcNanoSlice::Header::rtt_sync_delay_t dune::TpcNanoSlice::rtt_sync_delay() const
{
	uint32_t rtt_sync_delay = (header_()->raw_header_data[2]) & 0xFFFF;
	return static_cast<dune::TpcNanoSlice::Header::rtt_sync_delay_t>(rtt_sync_delay);
}

// Returns the specified hit bit map word from the header
dune::TpcNanoSlice::Header::hit_bit_map_t dune::TpcNanoSlice::hit_bit_map(
		dune::TpcNanoSlice::hit_bit_map_range const range) const
{
	uint64_t hit_bit_map = (header_()->raw_header_data[3 + range]);
	return static_cast<dune::TpcNanoSlice::Header::hit_bit_map_t>(hit_bit_map);
}

// Returns the sample count in the nanoslice
dune::TpcNanoSlice::sample_count_t dune::TpcNanoSlice::sampleCount() const
{
	// TODO When the nanoslice implements compression/sparsification, this will have
	// to inspect the data to determine the number of samples present
	return (dune::TpcNanoSlice::sample_count_t)(num_channels);
}

// Extracts a sample value from the encoded ADC data. Returns a boolean flag
// indicating if the decode succeeded (if requested channel in range). The decoded
// value is passed back in the specified argument
bool dune::TpcNanoSlice::sampleValue(uint32_t channel, uint16_t& value) const
{
  if (channel >= num_channels) {
    std::cout << "Requesting a channel number > "<< num_channels << std::endl;
    return false;
  }

//  if (this->data_compressed()) {
//
//	  int  index= (int)((channel*12)/64);
//	  int nstart = (channel*12-index*64);
//	  if(64-nstart>=12){
//		  value = (data_()[index]>>nstart)& 0xFFF; //right shift and read first 12 bits
//	  }
//	  else{
//		uint64_t lsb = (data_()[index]>>nstart);//right shift...don't need mask
//		int nibbleLength=12-(64-nstart);
//		uint64_t msb = (data_()[index+1]&getMask(nibbleLength))<<(12-nibbleLength); //compute mask for the nibble...
//		value=(uint16_t) msb|lsb;
//	  }
//  }
//  else
//  {
  int nstart;
  int index; 
  if(runMode==0x1){
    index = 0;
    nstart = 0;
  }  
  else{
    index = (int)(channel/4);
    nstart = (channel%4)*16;
  }
  value = (data_()[index] >> nstart) & 0xFFF;
  //  }
  return true;
}

// Returns a pointer to the raw data words in the nanoslice for diagnostics
uint64_t* dune::TpcNanoSlice::raw() const
{
	return reinterpret_cast<uint64_t*>(buffer_);
}

// Returns a pointer to the nanoslice header
dune::TpcNanoSlice::Header const* dune::TpcNanoSlice::header_() const
{
  return reinterpret_cast<Header const *>(buffer_);
}
//
// Returns a pointer to the first payload data word in the nanoslice
uint64_t const* dune::TpcNanoSlice::data_() const
{
    return reinterpret_cast<uint64_t const*>(buffer_ + sizeof(dune::TpcNanoSlice::Header));
}
