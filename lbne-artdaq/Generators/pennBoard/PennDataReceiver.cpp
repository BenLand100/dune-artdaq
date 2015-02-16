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

#include "lbne-raw-data/Overlays/PennMicroSlice.hh"

/*
struct PennMicrosliceHeader
{
  uint8_t  format_version;
  uint8_t  sequence_id;
  uint16_t block_size;
};
*/

//#define RECV_PENN_USLICE_IN_CHUNKS
#define RECV_DEBUG(level) if (level <= debug_level_) std::cout

lbne::PennDataReceiver::PennDataReceiver(int debug_level, uint32_t tick_period_usecs,
					 uint16_t receive_port, uint16_t number_of_microslices_per_millislice, bool rate_test) :
	debug_level_(debug_level),
	acceptor_(io_service_, tcp::endpoint(tcp::v4(), (short)receive_port)),
	accept_socket_(io_service_),
	data_socket_(io_service_),
	deadline_(io_service_),
	deadline_io_object_(None),
	tick_period_usecs_(tick_period_usecs),
	receive_port_(receive_port),
	number_of_microslices_per_millislice_(number_of_microslices_per_millislice),
	run_receiver_(true),
	suspend_readout_(false),
	readout_suspended_(false),
	recv_socket_(0),
	rate_test_(rate_test)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver constructor" << std::endl;

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
	RECV_DEBUG(1) << "lbne::PennDataReceiver destructor" << std::endl;

	// Flag receiver as no longer running
	run_receiver_.store(false);

	// Cancel any currently pending IO object deadlines and terminate timer
	deadline_.expires_at(boost::asio::deadline_timer::traits_type::now());

	// Wait for thread running receiver IO service to terminate
	receiver_thread_->join();

	RECV_DEBUG(1) << "lbne::PennDataReceiver destructor: receiver thread joined OK" << std::endl;

}

void lbne::PennDataReceiver::start(void)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver::start called" << std::endl;

	start_time_ = std::chrono::high_resolution_clock::now();

	// Clean up current buffer state of any partially-completed readouts in previous runs
	if (current_raw_buffer_ != nullptr)
	{
		RECV_DEBUG(2) << "TpcPennReceiver::start: dropping unused or partially filled buffer containing "
				      << microslices_recvd_ << " microslices" << std::endl;
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

	// Clear suspend readout handshake flags
	suspend_readout_.store(false);
	readout_suspended_.store(false);

}

void lbne::PennDataReceiver::stop(void)
{
	RECV_DEBUG(1) << "lbne::PennDataReceiver::stop called" << std::endl;

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
			std::cout << "ERROR - timeout waiting for PennDataReceiver thread to suspend readout" << std::endl;
			break;
		}
	}

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_recvd_) / elapsed_secs;

	RECV_DEBUG(0) << "lbne::PennDataRecevier::stop : last sequence id was " << (unsigned int)last_sequence_id_ << std::endl;
	RECV_DEBUG(0) << "lbne::PennDataReceiver::stop : received " << millislices_recvd_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz" << std::endl;

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
	RECV_DEBUG(1) << "lbne::PennDataReceiver::run_service starting" << std::endl;

	io_service_.run();

	RECV_DEBUG(1) << "lbne::PennDataReceiver::run_service stopping" << std::endl;
}

void lbne::PennDataReceiver::do_accept(void)
{
	// Suspend readout and cleanup any incomplete millislices if stop has been called
	if (suspend_readout_.load())
	{
		RECV_DEBUG(3) << "Suspending readout at do_accept entry" << std::endl;
		this->suspend_readout(false);
	}

	// Exit if shutting receiver down
	if (!run_receiver_.load()) {
		RECV_DEBUG(1) << "Stopping do_accept() at entry" << std::endl;
		return;
	}

	this->set_deadline(Acceptor, tick_period_usecs_);

	acceptor_.async_accept(accept_socket_,
		[this](boost::system::error_code ec)
		{
			if (!ec)
			{
				RECV_DEBUG(1) << "Accepted new data connection from source " << accept_socket_.remote_endpoint() << std::endl;
				data_socket_ = std::move(accept_socket_);
				this->do_read();
			}
			else
			{
				if (ec == boost::asio::error::operation_aborted)
				{
					RECV_DEBUG(3) << "Timeout on async_accept" << std::endl;
				}
				else
				{
				  std::cout << "Got error on asynchronous accept: " << ec << " " << ec.message() << std::endl;
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
		RECV_DEBUG(3) << "Suspending readout at do_read entry" << std::endl;
		this->suspend_readout(true);
	}

	// Terminate receiver read loop if required
	if (!run_receiver_.load())
	{
		RECV_DEBUG(1) << "Stopping do_read at entry" << std::endl;
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
				std::cout << "lbne::PennDataReceiver::receiverLoop no buffers available on commit queue" <<std::endl;;
			}
		} while (!buffer_available && (buffer_retries < max_buffer_retries));

		if (buffer_available)
		{
			millislice_state_ = MillisliceIncomplete;
			millislice_size_recvd_ = 0;
			microslices_recvd_ = 0;
			microslices_recvd_timestamp_ = 0;
			current_write_ptr_ = (void*)(current_raw_buffer_->dataPtr());
			RECV_DEBUG(2) << "Receiving new millislice into raw buffer at address " << current_write_ptr_ << std::endl;
		}
		else
		{
			std::cout << "Failed to obtain new raw buffer for millislice, terminating receiver loop" << std::endl;
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
		      << " next recv size " << next_receive_size_ << std::endl;
	
	// Start the asynchronous receive operation into the current raw buffer.
	data_socket_.async_receive(
		boost::asio::buffer(current_write_ptr_, next_receive_size_),
		[this](boost::system::error_code ec, std::size_t length)
		{
			if (!ec)
			{
				RECV_DEBUG(2) << "Received " << length << " bytes on socket" << std::endl;

				this->handle_received_data(length);

				this->do_read();
			}
			else
			{
				if (ec == boost::asio::error::eof)
				{
					RECV_DEBUG(1) << "Client socket closed the connection" << std::endl;
					this->do_accept();
				}
				else if (ec == boost::asio::error::operation_aborted)
				{
					RECV_DEBUG(3) << "Timeout on read from data socket" << std::endl;
					this->do_read();
				}
				else
				{
				  std::cout << "Got error on aysnchronous read: " << ec << " " << ec.message() << std::endl;
				}

			}
		}
	);
}

void lbne::PennDataReceiver::handle_received_data(std::size_t length)
{

	if(debug_level_ > 6) {
	  RECV_DEBUG(6) << "Bits received: ";
	  for(unsigned int i = 0; i < length; i++)
	    RECV_DEBUG(6) << std::bitset<8>(*((reinterpret_cast<uint8_t*>(current_write_ptr_))+i)) << " ";
	  RECV_DEBUG(6) << std::endl;
	}

	//update size of uslice component, uslice & mslice
	state_nbytes_recvd_    += length;
	microslice_size_recvd_ += length;
	millislice_size_recvd_ += length;

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
			<< " bytes, expected " << nbytes_expected << ")" << std::endl;
	  return;
	}

	switch (next_receive_state_)
	{
	case ReceiveMicrosliceHeader:
	  {

	    //byte swap the block_size before we create the Header
	    *((uint16_t*)state_start_ptr_ + 1) = ntohs(*((uint16_t*)state_start_ptr_ + 1));

	    // Capture the microslice version, length and sequence ID from the header
	    lbne::PennMicroSlice::Header* header;
	    header = reinterpret_cast<lbne::PennMicroSlice::Header*>(state_start_ptr_);

	    lbne::PennMicroSlice::Header::format_version_t microslice_version;
	    lbne::PennMicroSlice::Header::sequence_id_t    sequence_id;
	    microslice_version = header->format_version;
	    sequence_id        = header->sequence_id;
	    microslice_size_   = header->block_size;

	    RECV_DEBUG(2) << "Got header for microslice version 0x" << std::hex << (unsigned int)microslice_version << std::dec 
			  << " with size " << (unsigned int)microslice_size_
			  << " sequence ID " << (unsigned int)sequence_id
			  << std::endl;

	    RECV_DEBUG(5) << "Header bits: " << std::bitset<8>(microslice_version) << " " << std::bitset<8>(sequence_id) << " " << std::bitset<16>(microslice_size_) << std::endl;

	    // Validate the version - it shouldn't change!
	    if(microslice_version_initialised_ && (microslice_version != last_microslice_version_)) {
	      std::cout << "ERROR: Latest microslice version 0x" << std::hex << (unsigned int)microslice_version
			<< " is different to previous microslice version 0x" << (unsigned int)last_microslice_version_ << std::dec
			<< std::endl;
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
	      std::cout << "ERROR: Microslice version and version complement do not agree 0x"
			<< std::hex << (unsigned int)version << ", 0x" << (unsigned int)version_complement << std::dec << std::endl;
	      //TODO handle error cleanly here
	    }
	    
	    // Validate the sequence ID - should be incrementing monotonically (or identical to previous if it was fragmented)
	    if (sequence_id_initialised_ && (sequence_id != uint8_t(last_sequence_id_+1))) {
	      if (last_microslice_was_fragment_ && (sequence_id == uint8_t(last_sequence_id_))) {
		// do nothing - we're in a normal fragmented microslice
	      }
	      else if (last_microslice_was_fragment_ && (sequence_id != uint8_t(last_sequence_id_))) {
		std::cout << "WARNING: mismatch in microslice sequence IDs! Got " << (unsigned int)sequence_id << " expected " << (unsigned int)(uint8_t(last_sequence_id_)) << std::endl;
		//TODO handle error cleanly here
	      }
	      else if (rate_test_ && (sequence_id == uint8_t(last_sequence_id_))) {
		// do nothing - alll microslices in the rate test have the same sequence id
	      }
	      else {
		std::cout << "WARNING: mismatch in microslice sequence IDs! Got " << (unsigned int)sequence_id << " expected " << (unsigned int)(uint8_t(last_sequence_id_+1)) << std::endl;
		//TODO handle error cleanly here
	      }
	    }
	    else {
	      sequence_id_initialised_ = true;
	    }
	    last_sequence_id_ = sequence_id;

	    //update states ready to be for next call
	    millislice_state_ = MicrosliceIncomplete;
#ifdef RECV_PENN_USLICE_IN_CHUNKS
	    next_receive_state_ = ReceiveMicroslicePayloadHeader;
	    next_receive_size_  = sizeof(lbne::PennMicroSlice::Payload_Header);
#else
	    next_receive_state_ = ReceiveMicroslicePayload;
	    next_receive_size_  = microslice_size_ - sizeof(lbne::PennMicroSlice::Header);
#endif

	    //reset this since we're in a new uslice now
	    microslice_seen_timestamp_word_ = false;
	    last_microslice_was_fragment_ = false;

	    break;
	  } //case ReceiveMicrosliceHeader

#ifdef RECV_PENN_USLICE_IN_CHUNKS
	case ReceiveMicroslicePayloadHeader:
	  {
	    RECV_DEBUG(2) << "Got microslice payload header length " << state_nbytes_recvd_ << std::endl;
	    
	    //byte swap the data before we create the Payload_Header
            *((uint32_t*)state_start_ptr_) = ntohl(*((uint32_t*)state_start_ptr_));

	    // Capture the microslice payload type & timestamp from the payload header
	    lbne::PennMicroSlice::Payload_Header* payload_header;
	    payload_header = reinterpret_cast<lbne::PennMicroSlice::Payload_Header*>(state_start_ptr_);
	    uint8_t  type      = payload_header->data_packet_type;
	    uint32_t timestamp = payload_header->short_nova_timestamp;

	    RECV_DEBUG(2) << "Got header for microslice payload with type 0x" << std::hex << (unsigned int)type << std::dec 
			  << " and timestamp " << timestamp << std::endl;
	    
	    RECV_DEBUG(5) << "Payload header bits: " << std::bitset<4>(type) << " " << std::bitset<28>(timestamp) << std::endl;

            //update states ready to be for next call
	    switch(type)
	      {
	      case 0x01: // 0b0001
		next_receive_state_ = ReceiveMicroslicePayloadCounter;
		next_receive_size_  = lbne::PennMicroSlice::payload_size_counter;
		break;
	      case 0x02: // 0b0010
		next_receive_state_ = ReceiveMicroslicePayloadTrigger;
		next_receive_size_  = lbne::PennMicroSlice::payload_size_trigger;
		break;
	      case 0x08: // 0b1000
		next_receive_state_ = ReceiveMicroslicePayloadTimestamp;
		next_receive_size_  = lbne::PennMicroSlice::payload_size_timestamp;
		break;
	      default:
		RECV_DEBUG(1) << "WARNING: Unknown payload type 0x" << std::hex << (unsigned int)type << std::dec << std::endl;
		//TODO handle error cleanly here
	      }
	    millislice_state_ = MicrosliceIncomplete;
	    
	    break;
	  } //case ReceiveMicroslicePayloadHeader

	case ReceiveMicroslicePayloadTimestamp:
	  microslice_seen_timestamp_word_ = true;
	case ReceiveMicroslicePayloadCounter:
	case ReceiveMicroslicePayloadTrigger:
	  {
	    RECV_DEBUG(2) << "Got microslice payload length " << state_nbytes_recvd_ << std::endl;
	  
	    if (microslice_size_recvd_ == microslice_size_)
	      //got a full microslice
	      {
		RECV_DEBUG(2) << "Complete payload received for microslice " << microslices_recvd_ << std::endl;
		microslices_recvd_++;
		if(microslice_seen_timestamp_word_) {
		  microslices_recvd_timestamp_++;
		}
		else {
		  last_microslice_was_fragment_ = true;
		}
		microslice_size_recvd_ = 0;
		millislice_state_ = MillisliceIncomplete;
		next_receive_state_ = ReceiveMicrosliceHeader;
		next_receive_size_ = sizeof(lbne::PennMicroSlice::Header);
	      }
	    else
	      //the microslice isn't yet complete. go get another payload header
	      {
		RECV_DEBUG(2) << "Incomplete payload received for microslice " << microslices_recvd_
			      << " (got " << microslice_size_recvd_
			      << " expected " << microslice_size_ << " bytes)" << std::endl;
		millislice_state_ = MicrosliceIncomplete;
		next_receive_state_ = ReceiveMicroslicePayloadHeader;
		next_receive_size_ = sizeof(lbne::PennMicroSlice::Payload_Header);
	      }
	    break;
	  }//case ReceiveMicroslicePayload[Counter,Trigger,Timestamp]

#else
	case ReceiveMicroslicePayload:
	  {
	    //got a full microslice (complete size checks already done)
	    RECV_DEBUG(2) << "Complete payload received for microslice " << microslices_recvd_ << " length " << state_nbytes_recvd_ << std::endl;
	    microslices_recvd_++;

	    //form a microslice & check for the timestamp word to see if we're in a fragmented microslice
	    lbne::PennMicroSlice uslice(((uint8_t*)state_start_ptr_) - sizeof(lbne::PennMicroSlice::Header));
	    lbne::PennMicroSlice::sample_count_t n_counter_words(0);
	    lbne::PennMicroSlice::sample_count_t n_trigger_words(0);
	    lbne::PennMicroSlice::sample_count_t n_timestamp_words(0);
	    lbne::PennMicroSlice::sample_count_t n_words =
	      uslice.sampleCount(n_counter_words, n_trigger_words, n_timestamp_words, true);	    
	    RECV_DEBUG(2) << "Payload contains " << n_words
			  << " total words ("    << n_counter_words
			  << " counter + "       << n_trigger_words
			  << " trigger + "       << n_timestamp_words
			  << " timestamp)"       << std::endl;
	    if(n_timestamp_words) {
	      microslices_recvd_timestamp_++;
	      last_microslice_was_fragment_ = false;
	    }
	    else
	      last_microslice_was_fragment_ = true;

	    microslice_size_recvd_ = 0;
	    millislice_state_ = MillisliceIncomplete;
	    next_receive_state_ = ReceiveMicrosliceHeader;
	    next_receive_size_ = sizeof(lbne::PennMicroSlice::Header);

            break;
	  }//case ReceiveMicroslicePayload
#endif //RECV_PENN_USLICE_IN_CHUNKS

	default:
	  {
	    // Should never happen - bug or data corruption
	    std::cout << "FATAL ERROR after async_recv - unrecognised next receive state: " << next_receive_state_ << std::endl;
	    return;
	    break;
	  }
	}//switch(next_receive_state_)

	//reset counters & ptrs for the next call
	state_nbytes_recvd_ = 0;
	state_start_ptr_    = 0;

	// If correct number of microslices (with timestamps) have been received, flag millislice as complete
	if (microslices_recvd_timestamp_ == number_of_microslices_per_millislice_)
	{
		RECV_DEBUG(1) << "Millislice " << millislices_recvd_
			      << " complete with " << microslices_recvd_timestamp_
			      << " microslices (" << microslices_recvd_
			      << " including fragments), total size " << millislice_size_recvd_
			      << " bytes" << std::endl;
		millislices_recvd_++;
		millislice_state_ = MillisliceComplete;
	}

	// If the millislice is complete, place the buffer to the filled queue and set the state accordingly
	if (millislice_state_ == MillisliceComplete)
	{
		current_raw_buffer_->setSize(millislice_size_recvd_);
		current_raw_buffer_->setCount(microslices_recvd_);
		current_raw_buffer_->setFlags(0);
		filled_buffer_queue_.push(std::move(current_raw_buffer_));
		millislice_state_ = MillisliceEmpty;
	}
}

void lbne::PennDataReceiver::suspend_readout(bool await_restart)
{
	readout_suspended_.store(true);

	if (await_restart)
	{
		RECV_DEBUG(2) << "TpcPennReceiver::suspend_readout: awaiting restart or shutdown" << std::endl;
		while (suspend_readout_.load() && run_receiver_.load())
		{
			usleep(tick_period_usecs_);
		}
		RECV_DEBUG(2) << "TpcPennReceiver::suspend_readout: restart or shutdown detected, exiting wait loop" << std::endl;
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
		RECV_DEBUG(1) << "Deadline actor terminating" << std::endl;
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
