/*
 * PennDataReceiver.cpp - PENN data recevier classed based on the Boost asynchronous IO network library
 *
 *  Created on: Dec 15, 2014
 *      Author: tdealtry (based on tcn45 rce code)
 */

#include "PennDataReceiver.hh"

#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <bitset>
#include <boost/crc.hpp>

#include "lbne-raw-data/Overlays/PennMicroSlice.hh"
#include "lbne-raw-data/Overlays/Utilities.hh"

// JCF, Jul-14-2015: I promoted mf::LogDebug up to mf::LogInfo here

#define RECV_DEBUG(level) if (level <= debug_level_) mf::LogInfo("PennDataReceiver")


lbne::PennDataReceiver::PennDataReceiver(int debug_level, uint32_t tick_period_usecs,
					 uint16_t receive_port, uint32_t millislice_size, 
					 uint16_t millislice_overlap_size, bool rate_test ) :
	debug_level_(debug_level),
	acceptor_(io_service_, tcp::endpoint(tcp::v4(), (short)receive_port)),
	accept_socket_(io_service_),
	data_socket_(io_service_),
	deadline_(io_service_),
	deadline_io_object_(None),
	tick_period_usecs_(tick_period_usecs),
	receive_port_(receive_port),
	millislice_size_(millislice_size),
	run_receiver_(true),
	suspend_readout_(false),
	readout_suspended_(false),
	recv_socket_(0),
	rate_test_(rate_test),
	millislice_overlap_size_(millislice_overlap_size)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver constructor";

	//in this instance millislice_size_ is the number of bits that need to rollover to start a new microslice
	if((pow(millislice_size_, 2) - 1) > lbne::PennMicroSlice::ROLLOVER_LOW_VALUE) {
	  RECV_DEBUG(0) << "lbne::PennDataReceiver WARNING millislice_size_ " << millislice_size_
			<< " is greater than lbne::PennMicroSlice::ROLLOVER_LOW_VALUE " << (uint32_t)lbne::PennMicroSlice::ROLLOVER_LOW_VALUE
			<< " 28-bit timestamp rollover will not be handled correctly";
	  //TODO handle error cleanly
	}

	// Initialise and start the persistent deadline actor that handles socket operation
	// timeouts. Timeout is set to zero (no timeout) to start
	this->set_deadline(None, 0);
	this->check_deadline();

	// Start asynchronous accept handler
	this->do_accept();

	// Launch thread to run receiver IO service
	receiver_thread_ = std::unique_ptr<std::thread>(new std::thread(&lbne::PennDataReceiver::run_service, this));
}

lbne::PennDataReceiver::~PennDataReceiver()
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver destructor";

	// Flag receiver as no longer running
	run_receiver_.store(false);

	// Cancel any currently pending IO object deadlines and terminate timer
	deadline_.expires_at(boost::asio::deadline_timer::traits_type::now());

	// Wait for thread running receiver IO service to terminate
	receiver_thread_->join();

	RECV_DEBUG(1) << "lbne::PennDataReceiver destructor: receiver thread joined OK";

}

void lbne::PennDataReceiver::start(void)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver::start called";

	start_time_ = std::chrono::high_resolution_clock::now();

	// Clean up current buffer state of any partially-completed readouts in previous runs
	if (current_raw_buffer_ != nullptr)
	{
		RECV_DEBUG(2) << "TpcPennReceiver::start: dropping unused or partially filled buffer containing "
				      << microslices_recvd_ << " microslices";
		current_raw_buffer_.reset();
		millislice_state_ = MillisliceEmpty;
	}

	// Initialise millislice state
	millislice_state_ = MillisliceEmpty;
	millislices_recvd_ = 0;

	// Initialise sequence ID latch
	sequence_id_initialised_ = false;
	last_sequence_id_ = 0;

	// Initialise microslice version latch
	microslice_version_initialised_ = false;
	last_microslice_version_ = 0;

	// Initialise receive state to read a microslice header first
	next_receive_state_ = ReceiveMicrosliceHeader;
	next_receive_size_  = sizeof(lbne::PennMicroSlice::Header);

	// Initialise this to make sure we can count number of 'full' microslices
	microslice_seen_timestamp_word_ = false;
	//... and aren't shocked by repeated sequence IDs
	last_microslice_was_fragment_   = false;

	// Initalise to make sure we wait for the full Header
	state_nbytes_recvd_ = 0;
	state_start_ptr_ = 0;

	// Clear the times used for calculating millislice boundaries
	run_start_time_ = 0;
	boundary_time_  = 0;
	overlap_time_   = 0;

	// Clear the counters used for the remains of split uslices
	remaining_size_ = 0;
	remaining_payloads_recvd_ = 0;
	remaining_payloads_recvd_counter_ = 0;
	remaining_payloads_recvd_trigger_ = 0;
	remaining_payloads_recvd_timestamp_ = 0;
	remaining_payloads_recvd_selftest_ = 0;
	remaining_payloads_recvd_checksum_ = 0;

        // Clear the counters used for the overlap period
        overlap_size_ = 0;
        overlap_payloads_recvd_ = 0;
        overlap_payloads_recvd_counter_ = 0;
        overlap_payloads_recvd_trigger_ = 0;
        overlap_payloads_recvd_timestamp_ = 0;
        overlap_payloads_recvd_selftest_ = 0;
        overlap_payloads_recvd_checksum_ = 0;

	// Clear suspend readout handshake flags
	suspend_readout_.store(false);
	readout_suspended_.store(false);

}

void lbne::PennDataReceiver::stop(void)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver::stop called";

	// Suspend readout and wait for receiver thread to respond accordingly
	suspend_readout_.store(true);

	const uint32_t stop_timeout_usecs = 5000000;
	uint32_t max_timeout_count = stop_timeout_usecs / tick_period_usecs_;
	uint32_t timeout_count = 0;
	while (!readout_suspended_.load())
	{
		usleep(tick_period_usecs_);
		timeout_count++;
		if (timeout_count > max_timeout_count)
		{
			mf::LogError("PennDataReceiver") << "ERROR - timeout waiting for PennDataReceiver thread to suspend readout";
			break;
		}
	}

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_recvd_) / elapsed_secs;

	RECV_DEBUG(0) << "lbne::PennDataRecevier::stop : last sequence id was " << (unsigned int)last_sequence_id_;
	RECV_DEBUG(0) << "lbne::PennDataReceiver::stop : received " << millislices_recvd_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz";

}

void lbne::PennDataReceiver::commit_empty_buffer(PennRawBufferPtr& buffer)
{
	empty_buffer_queue_.push(std::move(buffer));
}

size_t lbne::PennDataReceiver::empty_buffers_available(void)
{
	return empty_buffer_queue_.size();
}

size_t lbne::PennDataReceiver::filled_buffers_available(void)
{
	return filled_buffer_queue_.size();
}

bool lbne::PennDataReceiver::retrieve_filled_buffer(PennRawBufferPtr& buffer, unsigned int timeout_us)
{
	bool buffer_available = true;

	// If timeout is specified, try to pop the buffer from the queue and time out, otherwise
	// block waiting for the queue to contain a buffer
	if (timeout_us == 0)
	{
		buffer = filled_buffer_queue_.pop();
	}
	else
	{
		buffer_available = filled_buffer_queue_.try_pop(buffer, std::chrono::microseconds(timeout_us));
	}

	return buffer_available;
}

void lbne::PennDataReceiver::release_empty_buffers(void)
{

	while (empty_buffer_queue_.size() > 0)
	{
		empty_buffer_queue_.pop();
	}
}

void lbne::PennDataReceiver::release_filled_buffers(void)
{
	while (filled_buffer_queue_.size() > 0)
	{
		filled_buffer_queue_.pop();
	}
}

void lbne::PennDataReceiver::run_service(void)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver::run_service starting";

	io_service_.run();

	RECV_DEBUG(1) << "lbne::PennDataReceiver::run_service stopping";
}

void lbne::PennDataReceiver::do_accept(void)
{
	// Suspend readout and cleanup any incomplete millislices if stop has been called
	if (suspend_readout_.load())
	{
		RECV_DEBUG(3) << "Suspending readout at do_accept entry";
		this->suspend_readout(false);
	}

	// Exit if shutting receiver down
	if (!run_receiver_.load()) {
		RECV_DEBUG(1) << "Stopping do_accept() at entry";
		return;
	}

	this->set_deadline(Acceptor, tick_period_usecs_);

	acceptor_.async_accept(accept_socket_,
		[this](boost::system::error_code ec)
		{
			if (!ec)
			{
				RECV_DEBUG(1) << "Accepted new data connection from source " << accept_socket_.remote_endpoint();
				data_socket_ = std::move(accept_socket_);
				this->do_read();
			}
			else
			{
				if (ec == boost::asio::error::operation_aborted)
				{
					RECV_DEBUG(3) << "Timeout on async_accept";
				}
				else
				{
				  mf::LogError("PennDataReceiver") << "Got error on asynchronous accept: " << ec << " " << ec.message();
				}
				this->do_accept();
			}
		}
       );
}

void lbne::PennDataReceiver::do_read(void)
{

	// Suspend readout and cleanup any incomplete millislices if stop has been called
	if (suspend_readout_.load())
	{
		RECV_DEBUG(3) << "Suspending readout at do_read entry";
		this->suspend_readout(true);
	}

	// Terminate receiver read loop if required
	if (!run_receiver_.load())
	{
		RECV_DEBUG(1) << "Stopping do_read at entry";
		return;
	}

	// Set timeout on read from data socket
	this->set_deadline(DataSocket, tick_period_usecs_);

	if (millislice_state_ == MillisliceEmpty)
	{

		// Attempt to obtain a raw buffer to receive data into
		unsigned int buffer_retries = 0;
		const unsigned int max_buffer_retries = 10;
		bool buffer_available;

		do
		{
			buffer_available = empty_buffer_queue_.try_pop(current_raw_buffer_, std::chrono::milliseconds(1000));
			if (!buffer_available)
			{
				buffer_retries++;
				mf::LogError("PennDataReceiver") << "lbne::PennDataReceiver::receiverLoop no buffers available on commit queue";
			}
		} while (!buffer_available && (buffer_retries < max_buffer_retries));

		if (buffer_available)
		{
			millislice_state_ = MillisliceIncomplete;
			millislice_size_recvd_ = 0;
			microslices_recvd_ = 0;
			microslices_recvd_timestamp_ = 0;
			payloads_recvd_ = 0;
			payloads_recvd_counter_ = 0;
			payloads_recvd_trigger_ = 0;
			payloads_recvd_timestamp_ = 0;
			payloads_recvd_selftest_  = 0;
			payloads_recvd_checksum_  = 0;
			current_write_ptr_ = (void*)(current_raw_buffer_->dataPtr());
			RECV_DEBUG(2) << "Receiving new millislice into raw buffer at address " << current_write_ptr_;

			//add the overlap period with the previous millislice to the start of this millislice
                        if(overlap_size_) {
			  //move the overlap period to the start of the new millislice
                          memmove(current_write_ptr_, overlap_ptr_, overlap_size_);
                          current_write_ptr_ = (void*)((char*)current_write_ptr_ + overlap_size_);
                          RECV_DEBUG(2) << "Overlap period of " << overlap_size_ << " bytes added to this millislice. "
                                        << "Payload contains "  << overlap_payloads_recvd_
                                        << " total words ("     << overlap_payloads_recvd_counter_
                                        << " counter + "        << overlap_payloads_recvd_trigger_
                                        << " trigger + "        << overlap_payloads_recvd_timestamp_
                                        << " timestamp + "      << overlap_payloads_recvd_selftest_
                                        << " selftest + "       << overlap_payloads_recvd_checksum_
                                        << "checksum)";
                          //increment size & payload counters
                          millislice_size_recvd_    += overlap_size_;
                          payloads_recvd_           += overlap_payloads_recvd_;
                          payloads_recvd_counter_   += overlap_payloads_recvd_counter_;
                          payloads_recvd_trigger_   += overlap_payloads_recvd_trigger_;
                          payloads_recvd_timestamp_ += overlap_payloads_recvd_timestamp_;
                          payloads_recvd_selftest_  += overlap_payloads_recvd_selftest_;
                          payloads_recvd_checksum_  += overlap_payloads_recvd_checksum_;
                          //reset 'overlap' counters
                          overlap_size_                     = 0;
                          overlap_payloads_recvd_           = 0;
                          overlap_payloads_recvd_counter_   = 0;
                          overlap_payloads_recvd_trigger_   = 0;
                          overlap_payloads_recvd_timestamp_ = 0;
                          overlap_payloads_recvd_selftest_  = 0;
                          overlap_payloads_recvd_checksum_  = 0;
                        }

			//add any remains of the previous microslice (due to millislice boundary occuring within it) to the start of this millislice
			if(remaining_size_) {
			  //TODO if the microslice time is wider than the millislice time, need to split the remains up, finalise a millislice, and call do_read() again (UNLIKELY)
			  memmove(current_write_ptr_, remaining_ptr_, remaining_size_);
			  current_write_ptr_ = (void*)((char*)current_write_ptr_ + remaining_size_);
			  RECV_DEBUG(2) << "Added the last "   << remaining_size_ << " bytes of previous microslice to this millislice. "
					<< "Payload contains " << remaining_payloads_recvd_
					<< " total words ("    << remaining_payloads_recvd_counter_
					<< " counter + "       << remaining_payloads_recvd_trigger_
					<< " trigger + "       << remaining_payloads_recvd_timestamp_
					<< " timestamp + "     << remaining_payloads_recvd_selftest_
					<< " selftest + "      << remaining_payloads_recvd_checksum_
					<< "checksum)";
			  //increment size & payload counters
			  millislice_size_recvd_    += remaining_size_;
			  payloads_recvd_           += remaining_payloads_recvd_;
			  payloads_recvd_counter_   += remaining_payloads_recvd_counter_;
			  payloads_recvd_trigger_   += remaining_payloads_recvd_trigger_;
			  payloads_recvd_timestamp_ += remaining_payloads_recvd_timestamp_;
			  payloads_recvd_selftest_  += remaining_payloads_recvd_selftest_;
			  payloads_recvd_checksum_  += remaining_payloads_recvd_checksum_;
			  //reset 'remaining' counters
			  remaining_size_                     = 0;
			  remaining_payloads_recvd_           = 0;
			  remaining_payloads_recvd_counter_   = 0;
			  remaining_payloads_recvd_trigger_   = 0;
			  remaining_payloads_recvd_timestamp_ = 0;
			  remaining_payloads_recvd_selftest_  = 0;
			  remaining_payloads_recvd_checksum_  = 0;
			}
		}
		else
		{
			mf::LogError("PennDataReceiver") << "Failed to obtain new raw buffer for millislice, terminating receiver loop";
			// TODO handle error cleanly here
			return;
		}

	}

	RECV_DEBUG(3) << "RECV: state "     << (unsigned int)next_receive_state_ << " " << nextReceiveStateToString(next_receive_state_)
		      << " mslice state "   << (unsigned int)millislice_state_   << " " << millisliceStateToString(millislice_state_)
		      << " uslice "         << microslices_recvd_
		      << " uslice size "    << microslice_size_recvd_
		      << " mslice size "    << millislice_size_recvd_
		      << " addr "           << current_write_ptr_
		      << " next recv size " << next_receive_size_;
	
	// Start the asynchronous receive operation into the current raw buffer.
	data_socket_.async_receive(
		boost::asio::buffer(current_write_ptr_, next_receive_size_),
		[this](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				RECV_DEBUG(2) << "Received " << length << " bytes on socket";

				this->handle_received_data(length);

				this->do_read();
			}
			else
			{
				if (ec == boost::asio::error::eof)
				{
					RECV_DEBUG(1) << "Client socket closed the connection";
					this->do_accept();
				}
				else if (ec == boost::asio::error::operation_aborted)
				{
					RECV_DEBUG(3) << "Timeout on read from data socket";
					this->do_read();
				}
				else
				{
				  mf::LogError("PennDataReceiver") << "Got error on aysnchronous read: " << ec << " " << ec.message();
				}

			}
		}
	);
}

void lbne::PennDataReceiver::handle_received_data(std::size_t length)
{

	//update size of uslice component, uslice & mslice
	state_nbytes_recvd_    += length;
	microslice_size_recvd_ += length;
	millislice_size_recvd_ += length;

	// JCF, Jul-18-2015

	// The four-byte chunks appear to be coming in in reverse
	// order from the ptb_reader program, thus the calls to ntohl

	for (decltype(length) i_l = 0; i_l < length/sizeof(uint32_t) ; ++i_l) {
	  *(static_cast<uint32_t*>(current_write_ptr_)+i_l) = ntohl(*(static_cast<uint32_t*>(current_write_ptr_)+i_l));
	}

	
	display_bits(current_write_ptr_, length, "PennDataReceiver");

	//make pointer to the start of the current object (Header, Payload_Header, Payload*)
	if(!state_start_ptr_)
	  state_start_ptr_ = current_write_ptr_;

	//now we can update the current_write_ptr_
	current_write_ptr_ = (void*)((char*)current_write_ptr_ + length);

	//check to see if we have all the bytes for the current object
	std::size_t nbytes_expected = nextReceiveStateToExpectedBytes(next_receive_state_);
	if(state_nbytes_recvd_ < nbytes_expected) {
	  millislice_state_   = MicrosliceIncomplete;
	  //next_receive_state_ = //already set correctly
	  next_receive_size_ = nbytes_expected - state_nbytes_recvd_;
	  RECV_DEBUG(2) << "Incomplete " << nextReceiveStateToString(next_receive_state_)
			<< " received for microslice " << microslices_recvd_
			<< " (got " << state_nbytes_recvd_
			<< " bytes, expected " << nbytes_expected << ")";
	  return;
	}

	switch (next_receive_state_)
	{
	case ReceiveMicrosliceHeader:
	  {

	    RECV_DEBUG(2) << "JCF: at start of \"case ReceiveMicrosliceHeader\"";
	    
	    // JCF, Jul-19-2015

	    // x86 systems use Little Endian, so byte swap the
	    // block_size before we create the Header

	    *(static_cast<uint16_t*>(state_start_ptr_)+1) = ntohs(*(static_cast<uint16_t*>(state_start_ptr_)+1));

	    display_bits( static_cast<void*>(state_start_ptr_), length, "PennDataReceiver");

	    // Capture the microslice version, length and sequence ID from the header
	    lbne::PennMicroSlice::Header* header = reinterpret_cast_checked<lbne::PennMicroSlice::Header*>( state_start_ptr_ );

	    lbne::PennMicroSlice::Header::format_version_t microslice_version;
	    lbne::PennMicroSlice::Header::sequence_id_t    sequence_id;
	    microslice_version = header->format_version;
	    sequence_id        = header->sequence_id;
	    microslice_size_   = header->block_size;

	    RECV_DEBUG(2) << "Got header for microslice version 0x" << std::hex << (unsigned int)microslice_version << std::dec 
			  << " with size " << (unsigned int)microslice_size_
			  << " sequence ID " << (unsigned int)sequence_id;

	    //	    RECV_DEBUG(4) << "Header bits: " << std::bitset<8>(microslice_version) << " " << std::bitset<8>(sequence_id) << " " << std::bitset<16>(microslice_size_);

	    // Validate the version - it shouldn't change!
	    if(microslice_version_initialised_ && (microslice_version != last_microslice_version_)) {
	      mf::LogError("PennDataReceiver") << "ERROR: Latest microslice version 0x" << std::hex << (unsigned int)microslice_version
			<< " is different to previous microslice version 0x" << (unsigned int)last_microslice_version_ << std::dec;
	      //TODO handle error cleanly here
	    }
	    else{
	      microslice_version_initialised_ = true;
	    }
	    last_microslice_version_ = microslice_version;

	    // Validate the version - should be a 4-bit version & 4-bit version complement
	    uint8_t version            =  microslice_version & 0x0F;
	    uint8_t version_complement = (microslice_version & 0xF0) >> 4;
	    if( ! ((version ^ version_complement) << 4) ) {
	      mf::LogError("PennDataReceiver") << "ERROR: Microslice version and version complement do not agree 0x"
			<< std::hex << (unsigned int)version << ", 0x" << (unsigned int)version_complement << std::dec;
	      //TODO handle error cleanly here
	    }
	    
	    // Validate the sequence ID - should be incrementing monotonically (or identical to previous if it was fragmented)
	    if (sequence_id_initialised_ && (sequence_id != uint8_t(last_sequence_id_+1))) {
	      if (last_microslice_was_fragment_ && (sequence_id == uint8_t(last_sequence_id_))) {
		// do nothing - we're in a normal fragmented microslice
	      }
	      else if (last_microslice_was_fragment_ && (sequence_id != uint8_t(last_sequence_id_))) {
		mf::LogError("PennDataReceiver") << "WARNING: mismatch in microslice sequence IDs! Got " << (unsigned int)sequence_id << " expected " << (unsigned int)(uint8_t(last_sequence_id_));
		//TODO handle error cleanly here
	      }
	      else if (rate_test_ && (sequence_id == uint8_t(last_sequence_id_))) {
		// do nothing - alll microslices in the rate test have the same sequence id
	      }
	      else {
		mf::LogError("PennDataReceiver") << "WARNING: mismatch in microslice sequence IDs! Got " << (unsigned int)sequence_id << " expected " << (unsigned int)(uint8_t(last_sequence_id_+1));
		//TODO handle error cleanly here
	      }
	    }
	    else {
	      sequence_id_initialised_ = true;
	    }
	    last_sequence_id_ = sequence_id;

	    //update states ready to be for next call
	    millislice_state_ = MicrosliceIncomplete;

	    next_receive_state_ = ReceiveMicroslicePayload;
	    next_receive_size_  = microslice_size_ - sizeof(lbne::PennMicroSlice::Header);

	    //reset this since we're in a new uslice now
	    microslice_seen_timestamp_word_ = false;
	    last_microslice_was_fragment_ = false;

	    //and roll back the current_write_ptr_, as to overwrite the Header in the next recv
	    current_write_ptr_ = (void*)((char*)current_write_ptr_ - sizeof(lbne::PennMicroSlice::Header));
	    millislice_size_recvd_ -= sizeof(lbne::PennMicroSlice::Header);

	    //copy the microslice header to memory, for checksum tests
	    memcpy(current_microslice_ptr_, state_start_ptr_, sizeof(lbne::PennMicroSlice::Header));

	    RECV_DEBUG(2) << "JCF: about to reach end of \"case ReceiveMicrosliceHeader\"";
	    break;
	  } //case ReceiveMicrosliceHeader

	case ReceiveMicroslicePayload:
	  {
	    //got a full microslice (complete size checks already done)
	    RECV_DEBUG(2) << "Complete payload received for microslice " << microslices_recvd_ << " length " << state_nbytes_recvd_;
	    microslices_recvd_++;

	    //TODO add a better way of getting a start time, to calculate millislice boundaries from (presumably read the Penn)
	    
	    // JCF, Jul-21-2015

	    // Please check to see what the address of the
	    // state_start_ptr_ is here. The broader question being,
	    // what's meant by the run_start_time_, and why is it
	    // measured in units of size?

	    if(!run_start_time_) {
	      run_start_time_ = ntohl(*((uint32_t*)state_start_ptr_))        & 0xFFFFFFF; //lowest 28 bits
	      boundary_time_  = (run_start_time_ + millislice_size_ - 1)     & 0xFFFFFFF; //lowest 28 bits
	      overlap_time_   = (boundary_time_  - millislice_overlap_size_) & 0xFFFFFFF; //lowest 28 bits
	    }

	    //form a microslice & check for the timestamp word to see if we're in a fragmented microslice
	    lbne::PennMicroSlice uslice(((uint8_t*)state_start_ptr_));

	    //count the number of different types of payload word
	    lbne::PennMicroSlice::sample_count_t n_counter_words(0);
	    lbne::PennMicroSlice::sample_count_t n_trigger_words(0);
	    lbne::PennMicroSlice::sample_count_t n_timestamp_words(0);
	    lbne::PennMicroSlice::sample_count_t n_selftest_words(0);
	    lbne::PennMicroSlice::sample_count_t n_checksum_words(0);
	    lbne::PennMicroSlice::sample_count_t n_words(0);

	    //also check to see if the millislice boundary is inside this microslice

	    uint32_t hardware_checksum(0);
	    std::size_t this_overlap_size(0);
	    uint8_t* this_overlap_ptr = nullptr;

	    // JCF, Jul-19-2015

	    // I set the second-to-last argument to false, telling
	    // sampleTimeSplitAndCountTwice NOT to reverse the bytes
	    // in the header - because I've added this feature already

	    uint8_t* split_ptr = 
	      uslice.sampleTimeSplitAndCountTwice(boundary_time_, remaining_size_,
						  overlap_time_,  this_overlap_size, this_overlap_ptr,
						  n_words, n_counter_words, n_trigger_words, n_timestamp_words, n_selftest_words, n_checksum_words,
						  remaining_payloads_recvd_, remaining_payloads_recvd_counter_, remaining_payloads_recvd_trigger_,
						  remaining_payloads_recvd_timestamp_, remaining_payloads_recvd_selftest_, remaining_payloads_recvd_checksum_,
						  overlap_payloads_recvd_, overlap_payloads_recvd_counter_, overlap_payloads_recvd_trigger_,
						  overlap_payloads_recvd_timestamp_, overlap_payloads_recvd_selftest_, overlap_payloads_recvd_checksum_,
						  hardware_checksum,
						  false, microslice_size_);


	    //WARNING this assumes that the last 64 bytes of a microslice is always a payload header & a checksum word
	    //TODO change the size if the checksum format changes
	    //checksum the microslice we've just received
	    // first copy the payload to the same location as the header
	    memcpy(current_microslice_ptr_ + sizeof(lbne::PennMicroSlice::Header), state_start_ptr_, state_nbytes_recvd_);
	    // then calculate the checksum ourself
	    boost::crc_32_type crc_checksum;
	    std::size_t microslice_size_to_checksum = sizeof(lbne::PennMicroSlice::Header) + state_nbytes_recvd_ - sizeof(lbne::PennMicroSlice::Payload_Header) - lbne::PennMicroSlice::payload_size_checksum;
	    crc_checksum.process_bytes(current_microslice_ptr_, microslice_size_to_checksum);
	    uint32_t software_checksum = crc_checksum.checksum();
	    // check they agree
	    if(hardware_checksum != software_checksum) {
	      mf::LogError("PennDataReceiver") << "ERROR: Microslice checksum mismatch! Hardware: " << hardware_checksum << " Software: " << software_checksum;
	      //TODO add error cleanly here
	    }
	    else
	      RECV_DEBUG(4) << "Microslice checksums... Hardware: " << hardware_checksum << " Software: " << software_checksum;

	    //make sure to remove the microslice checksum word from the millislice (it is useless without the header)
	    //TODO tweak this logic more - some microslice checksum words are still getting into the millislice
	    current_write_ptr_ = (void*)((char*)current_write_ptr_ - sizeof(lbne::PennMicroSlice::Payload_Header) - lbne::PennMicroSlice::payload_size_checksum);
	    millislice_size_recvd_ -= (sizeof(lbne::PennMicroSlice::Payload_Header) + lbne::PennMicroSlice::payload_size_checksum);
	    n_checksum_words--;
	    n_words--;
	    if(split_ptr != nullptr) {
	      if(remaining_payloads_recvd_checksum_) {
		remaining_size_   -= sizeof(lbne::PennMicroSlice::Payload_Header) - lbne::PennMicroSlice::payload_size_checksum;
		remaining_payloads_recvd_ -= remaining_payloads_recvd_checksum_;
		remaining_payloads_recvd_checksum_ = 0;
	      }
	    }
	    if(this_overlap_ptr != nullptr) {
	      if(overlap_payloads_recvd_checksum_) {
		this_overlap_size -= sizeof(lbne::PennMicroSlice::Payload_Header) - lbne::PennMicroSlice::payload_size_checksum;
		overlap_payloads_recvd_ -= overlap_payloads_recvd_checksum_;
		overlap_payloads_recvd_checksum_ = 0;
	      }
	    }

	    //stash the microslice data that's for the next millislice
            if(split_ptr != nullptr) {
		if(remaining_size_ > lbne::PennDataReceiver::remaining_buffer_size) {
		  mf::LogError("PennDataReceiver") << "ERROR buffer overflow for 'remaining bytes of microslice, after the millislice boundary'";
		  //TODO handle error cleanly here (also find out the largest possible microslice size (multiple-fragments) & set remaining_buffer_size appropriately)
		}
              RECV_DEBUG(2) << "Millislice boundary found within microslice " << microslices_recvd_timestamp_
                            << ". Storing " << remaining_size_ << " bytes for next millislice";
              memmove(remaining_ptr_, split_ptr, remaining_size_);
              millislice_size_recvd_ -= remaining_size_;
            }

	    //stash the microslice data that's for the overlap period at the start of the next millislice
	    if(this_overlap_ptr != nullptr) {
	      if(overlap_size_ + this_overlap_size > lbne::PennDataReceiver::overlap_buffer_size_) {
		mf::LogError("PennDataReceiver") << "ERROR buffer overflow for 'overlap bytes of microslice, after the millislice boundary'";
		//TODO handle error cleanly here (also find out the largest possible overlap size & set overlap_buffer_size_ appropriately)
	      }
	      RECV_DEBUG(2) << "Overlap period found within microslice " << microslices_recvd_timestamp_
                            << ". Storing " << overlap_size_ << " bytes for start of next millislice";
	      memcpy(overlap_ptr_ + overlap_size_, this_overlap_ptr, this_overlap_size);
	      overlap_size_ += this_overlap_size;
	    }

	    RECV_DEBUG(2) << "Payload contains " << n_words
			  << " total words ("    << n_counter_words
			  << " counter + "       << n_trigger_words
			  << " trigger + "       << n_timestamp_words
			  << " timestamp + "     << n_selftest_words
			  << " selftest + "      << n_checksum_words
			  << "checksum)"
			  << " before the millislice boundary";

	    //check if we're inside a fragmented microslice
	    if(n_timestamp_words) {
	      microslices_recvd_timestamp_++;
	      last_microslice_was_fragment_ = false;
	    }
	    else
	      last_microslice_was_fragment_ = true;

	    //increment payload counters
	    payloads_recvd_           += n_words;
	    payloads_recvd_counter_   += n_counter_words;
	    payloads_recvd_trigger_   += n_trigger_words;
	    payloads_recvd_timestamp_ += n_timestamp_words;
	    payloads_recvd_selftest_  += n_selftest_words;
	    payloads_recvd_checksum_  += n_checksum_words;

	    microslice_size_recvd_ = 0;
	    millislice_state_ = MillisliceIncomplete;
	    next_receive_state_ = ReceiveMicrosliceHeader;
	    next_receive_size_ = sizeof(lbne::PennMicroSlice::Header);

	    RECV_DEBUG(2) << "JCF: at end of \"case ReceiveMicroslicePayload\"";
            break;
	  }//case ReceiveMicroslicePayload

	default:
	  {
	    // Should never happen - bug or data corruption
	    mf::LogError("PennDataReceiver") << "FATAL ERROR after async_recv - unrecognised next receive state: " << next_receive_state_;
	    return;
	    break;
	  }
	}//switch(next_receive_state_)

	//reset counters & ptrs for the next call
	state_nbytes_recvd_ = 0;
	state_start_ptr_    = 0;

	// If correct number of microslices (with timestamps) have been received, flag millislice as complete

	if(remaining_size_)
	{
		RECV_DEBUG(1) << "Millislice " << millislices_recvd_
			      << " complete with " << microslices_recvd_timestamp_
			      << " microslices (" << microslices_recvd_
			      << " including fragments), total size " << millislice_size_recvd_
			      << " bytes and "      << payloads_recvd_
			      << " payload words (" << payloads_recvd_counter_
			      << " counter + "      << payloads_recvd_trigger_
			      << " trigger + "      << payloads_recvd_timestamp_
			      << " timestamp + "    << payloads_recvd_selftest_
			      << " selftest + "     << payloads_recvd_checksum_
			      << " checksum)";
		millislices_recvd_++;
		millislice_state_ = MillisliceComplete;
	}

	// If the millislice is complete, place the buffer to the filled queue and set the state accordingly
	if (millislice_state_ == MillisliceComplete)
	{
		current_raw_buffer_->setSize(millislice_size_recvd_);
		current_raw_buffer_->setCount(microslices_recvd_);
		current_raw_buffer_->setSequenceID(millislices_recvd_ & 0xFFFF); //lowest 16 bits
		current_raw_buffer_->setCountPayload(payloads_recvd_);
		current_raw_buffer_->setCountPayloadCounter(payloads_recvd_counter_);
		current_raw_buffer_->setCountPayloadTrigger(payloads_recvd_trigger_);
		current_raw_buffer_->setCountPayloadTimestamp(payloads_recvd_timestamp_);
		current_raw_buffer_->setEndTimestamp(boundary_time_);
		current_raw_buffer_->setWidthTicks(millislice_size_);
		current_raw_buffer_->setOverlapTicks(millislice_overlap_size_);
		current_raw_buffer_->setFlags(0);

		//update the times

		boundary_time_ = (boundary_time_ + millislice_size_)         & 0xFFFFFFF; //lowest 28 bits
		overlap_time_  = (boundary_time_ - millislice_overlap_size_) & 0xFFFFFFF; //lowest 28 bits
		filled_buffer_queue_.push(std::move(current_raw_buffer_));
		millislice_state_ = MillisliceEmpty;
	}
}

void lbne::PennDataReceiver::suspend_readout(bool await_restart)
{
	readout_suspended_.store(true);

	if (await_restart)
	{
		RECV_DEBUG(2) << "TpcPennReceiver::suspend_readout: awaiting restart or shutdown";
		while (suspend_readout_.load() && run_receiver_.load())
		{
			usleep(tick_period_usecs_);
		}
		RECV_DEBUG(2) << "TpcPennReceiver::suspend_readout: restart or shutdown detected, exiting wait loop";
	}

}

void lbne::PennDataReceiver::set_deadline(DeadlineIoObject io_object, unsigned int timeout_usecs)
{

	// Set the current IO object that the deadline is being used for
	deadline_io_object_ = io_object;

	// Set the deadline for the asynchronous write operation if the timeout is set, otherwise
	// revert back to +ve infinity to stall the deadline timer actor
	if (timeout_usecs > 0)
	{
		deadline_.expires_from_now(boost::posix_time::microseconds(timeout_usecs));
	}
	else
	{
		deadline_.expires_from_now(boost::posix_time::pos_infin);
	}
}

void lbne::PennDataReceiver::check_deadline(void)
{

	// Handle deadline if expired
	if (deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now())
	{

		// Cancel pending operation on current specified object
		switch (deadline_io_object_)
		{
		case Acceptor:
			acceptor_.cancel();
			break;
		case DataSocket:
			data_socket_.cancel();
			break;
		case None:
			// Do nothing
			break;
		default:
			// Shouldn't happen
			break;
		}

		// No longer an active deadline to set the expiry to positive inifinity
		deadline_.expires_at(boost::posix_time::pos_infin);
	}

	// Put the deadline actor back to sleep if receiver is still running
	if (run_receiver_.load())
	{
		deadline_.async_wait(boost::bind(&lbne::PennDataReceiver::check_deadline, this));
	}
	else
	{
		RECV_DEBUG(1) << "Deadline actor terminating";
	}
}

std::string lbne::PennDataReceiver::nextReceiveStateToString(NextReceiveState val)
{
  try {
    return next_receive_state_names_.at(val);
  }
  catch(const std::out_of_range& e) {
    return "INVALID/UNKNOWN";
  }
  return "INVALID/UNKNOWN";
}

std::string lbne::PennDataReceiver::millisliceStateToString(MillisliceState val)
{
  try {
    return millislice_state_names_.at(val);
  }
  catch(const std::out_of_range& e) {
    return "INVALID/UNKNOWN";
  }
  return "INVALID/UNKNOWN";
}

std::size_t lbne::PennDataReceiver::nextReceiveStateToExpectedBytes(NextReceiveState val)
{
  switch(val)
    {
    case ReceiveMicrosliceHeader:
      return sizeof(lbne::PennMicroSlice::Header);
      break;
#ifndef RECV_PENN_USLICE_IN_CHUNKS
    case ReceiveMicroslicePayload:
      return microslice_size_ - sizeof(lbne::PennMicroSlice::Header);
      break;
#else
    case ReceiveMicroslicePayloadHeader:
      return sizeof(lbne::PennMicroSlice::Payload_Header);
      break;
    case ReceiveMicroslicePayloadCounter:
      return (std::size_t)lbne::PennMicroSlice::payload_size_counter;
      break;
    case ReceiveMicroslicePayloadTrigger:
      return (std::size_t)lbne::PennMicroSlice::payload_size_trigger;
      break;
    case ReceiveMicroslicePayloadTimestamp:
      return (std::size_t)lbne::PennMicroSlice::payload_size_timestamp;
      break;
#endif
    default:
      return 0;
      break;
    }
}
