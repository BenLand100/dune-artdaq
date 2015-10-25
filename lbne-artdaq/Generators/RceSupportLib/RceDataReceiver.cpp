/*
 * RceDataReceiver.cpp - RCE data recevier classed based on the Boost asynchronous IO network library
 *
 *  Created on: May 16, 2014
 *      Author: Tim Nicholls, STFC Application Engineering Group
 */

#include "RceDataReceiver.hh"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"

#include <iostream>
#include <unistd.h>

struct RceMicrosliceHeader
{
  uint32_t microslice_size;
  uint32_t sequence_id;
  uint32_t type_id;
  uint32_t sw_frame_status[2];
  uint32_t fw_frame_status[2];
};

#define RECV_DEBUG(level) if (level <= debug_level_) DAQLogger::LogDebug(instance_name_)

lbne::RceDataReceiver::RceDataReceiver(const std::string& instance_name, int debug_level, uint32_t tick_period_usecs,
		uint16_t receive_port, uint16_t number_of_microslices_per_millislice) :
	instance_name_(instance_name),
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
	recv_socket_(0)
{
	RECV_DEBUG(1) << "lbne::RceDataReceiver constructor";

	// Initialise and start the persistent deadline actor that handles socket operation
	// timeouts. Timeout is set to zero (no timeout) to start
	this->set_deadline(None, 0);
	this->check_deadline();

	// Start asynchronous accept handler
	this->do_accept();

	// Launch thread to run receiver IO service
	receiver_thread_ = std::unique_ptr<std::thread>(new std::thread(&lbne::RceDataReceiver::run_service, this));

}

lbne::RceDataReceiver::~RceDataReceiver()
{
	RECV_DEBUG(1) << "lbne::RceDataReceiver destructor";

	// Flag receiver as no longer running
	run_receiver_.store(false);

	// Cancel any currently pending IO object deadlines and terminate timer
	deadline_.expires_at(boost::asio::deadline_timer::traits_type::now());

	// Wait for thread running receiver IO service to terminate
	receiver_thread_->join();

	RECV_DEBUG(1) << "lbne::RceDataReceiver destructor: receiver thread joined OK";

}

void lbne::RceDataReceiver::start(void)
{
	RECV_DEBUG(1) << "lbne::RceDataReceiver::start called";

	start_time_ = std::chrono::high_resolution_clock::now();

	// Clean up current buffer state of any partially-completed readouts in previous runs
	if (current_raw_buffer_ != nullptr)
	{
		RECV_DEBUG(2) << "TpcRceReceiver::start: dropping unused or partially filled buffer containing "
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

	// Initialise receive state to read a microslice header first
	next_receive_state_ = ReceiveMicrosliceHeader;
	next_receive_size_  = sizeof(RceMicrosliceHeader);

	// Clear suspend readout handshake flags
	suspend_readout_.store(false);
	readout_suspended_.store(false);

}

void lbne::RceDataReceiver::stop(void)
{
	RECV_DEBUG(1) << "lbne::RceDataReceiver::stop called";

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

		  // JCF, Oct-24-2015

		  // May want to downgrade this to a warning; in the
		  // meantime, swallow the exception this
		  // automatically throws

		  try {
			DAQLogger::LogError(instance_name_) << "Timeout waiting for RceDataReceiver thread to suspend readout";
		  } catch (...) {
		  }
			break;
		}
	}

	auto elapsed_msecs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time_).count();
	double elapsed_secs = ((double)elapsed_msecs) / 1000;
	double rate = ((double)millislices_recvd_) / elapsed_secs;

	RECV_DEBUG(0) << "lbne::RceDataRecevier::stop : last sequence id was " << last_sequence_id_;
	RECV_DEBUG(0) << "lbne::RceDataReceiver::stop : received " << millislices_recvd_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz";

}

void lbne::RceDataReceiver::commit_empty_buffer(RceRawBufferPtr& buffer)
{
	empty_buffer_queue_.push(std::move(buffer));
}

size_t lbne::RceDataReceiver::empty_buffers_available(void)
{
	return empty_buffer_queue_.size();
}

size_t lbne::RceDataReceiver::filled_buffers_available(void)
{
	return filled_buffer_queue_.size();
}

bool lbne::RceDataReceiver::retrieve_filled_buffer(RceRawBufferPtr& buffer, unsigned int timeout_us)
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

void lbne::RceDataReceiver::release_empty_buffers(void)
{

	while (empty_buffer_queue_.size() > 0)
	{
		empty_buffer_queue_.pop();
	}
}

void lbne::RceDataReceiver::release_filled_buffers(void)
{
	while (filled_buffer_queue_.size() > 0)
	{
		filled_buffer_queue_.pop();
	}
}

void lbne::RceDataReceiver::run_service(void)
{
	RECV_DEBUG(1) << "lbne::RceDataReceiver::run_service starting";

	io_service_.run();

	RECV_DEBUG(1) << "lbne::RceDataReceiver::run_service stopping";
}

void lbne::RceDataReceiver::do_accept(void)
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
					DAQLogger::LogError(instance_name_) << "Got error on asynchronous accept: " << ec;
				}
				this->do_accept();
			}
		}
	);
}

void lbne::RceDataReceiver::do_read(void)
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
				DAQLogger::LogWarning(instance_name_) << "lbne::RceDataReceiver::receiverLoop no buffers available on commit queue";
			}
		} while (!buffer_available && (buffer_retries < max_buffer_retries));

		if (buffer_available)
		{
			millislice_state_ = MillisliceIncomplete;
			millislice_size_recvd_ = 0;
			microslices_recvd_ = 0;
			microslice_size_recvd_ = 0;
			current_write_ptr_ = (void*)(current_raw_buffer_->dataPtr());
			current_header_ptr_ = current_write_ptr_;
			RECV_DEBUG(2) << "Receiving new millislice into raw buffer at address " << current_write_ptr_;
		}
		else
		{

		  try {
		    DAQLogger::LogError(instance_name_) << "Failed to obtain new raw buffer for millislice, terminating receiver loop";
		  } catch (...) {

		    // JCF, Oct-24-15
		 
		    // Swallow the exception... don't want to bring
		    // down the DAQ if the above error message throws
		    // an exception during the stop transition, as
		    // this means the output *.root file is in danger
		    // of not properly being closed
   
		    // However:

		    // At some point we may want to uncomment the code
		    // below, which basically means that if we can't
		    // obtain a new raw buffer for the millislice and
		    // we're in the running state, that it's a fatal
		    // error which SHOULD throw an exception; since
		    // "suspend_readout" isn't true, it means the
		    // exception ought to be captured by
		    // CommandableFragmentGenerator::getNext(),
		    // resulting in the output file being properly
		    // closed

		    // if (! suspend_readout_.load()) {
		    // ExceptionHandler(ExceptionHandlerRethrow::yes,
		    // "");
		    // }
		  }

		  return;
		}

	}

	RECV_DEBUG(3) << "RECV: state " << (unsigned int)next_receive_state_
			  	  << " mslice state " << (unsigned int)millislice_state_
			  	  << " uslice " << microslices_recvd_
			  	  << " uslice size " << microslice_size_recvd_
			  	  << " mslice size " << millislice_size_recvd_
			  	  << " addr " << current_write_ptr_
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
					DAQLogger::LogError(instance_name_) << "Got error on aysnchronous read: " << ec;
					DAQLogger::LogError(instance_name_) 
					              << "RECV: state " << (unsigned int)next_receive_state_
						      << " mslice state " << (unsigned int)millislice_state_
						      << " uslice " << microslices_recvd_
						      << " uslice size " << microslice_size_recvd_
						      << " mslice size " << millislice_size_recvd_
						      << " addr " << current_write_ptr_
						      << " next recv size " << next_receive_size_;

				}

			}
		}
	);
}

void lbne::RceDataReceiver::handle_received_data(std::size_t length)
{

	RceMicrosliceHeader* header;
	uint32_t sequence_id;

	microslice_size_recvd_ += length;

	switch (next_receive_state_)
	{
	case ReceiveMicrosliceHeader:

	  if (microslice_size_recvd_ == sizeof(RceMicrosliceHeader))
	  {
	    // Capture the microslice length and sequence ID from the header
	    header = reinterpret_cast<RceMicrosliceHeader*>(current_header_ptr_);
	    
	    microslice_size_ = header->microslice_size;
	    sequence_id = header->sequence_id;
	    
	    RECV_DEBUG(2) << "Got header for microslice with size " << microslice_size_ << " sequence ID " << sequence_id;
	    
	    // Validate the sequence ID - should be incrementing monotonically
	    if (sequence_id_initialised_ && (sequence_id != last_sequence_id_+1))
	    {
	      DAQLogger::LogWarning(instance_name_) << "Got mismatch in microslice sequence IDs! Got " << sequence_id << " expected " << last_sequence_id_+1;
	      DAQLogger::LogWarning(instance_name_) << "Receive length at mismatch : " << length;
	      DAQLogger::LogWarning(instance_name_) << "Microslice header at mismatch :"
					     << " size : 0x"         << std::hex << header->microslice_size << std::dec
					     << " sequence ID : 0x " << std::hex << header->sequence_id     << std::dec
					     << " type ID : 0x"      << std::hex << header->type_id         << std::dec
					     << " sw status: 0x"     << std::hex  
					     << ((uint64_t)(header->sw_frame_status[0]) << 32 | (header->sw_frame_status[1])) << std::dec
					     << " fw status: 0x"     << std::hex 
					     << ((uint64_t)(header->fw_frame_status[0]) << 32 | (header->fw_frame_status[1])) << std::dec;

		//TODO handle error cleanly here
	    }
	    else
	    {
	      sequence_id_initialised_ = true;
	    }

	    last_sequence_id_ = sequence_id;

	    millislice_state_ = MicrosliceIncomplete;
	    next_receive_state_ = ReceiveMicroslicePayload;
	    next_receive_size_ = microslice_size_ - sizeof(RceMicrosliceHeader);
	  }
	  else
	  {
	    millislice_state_   = MicrosliceIncomplete;
	    next_receive_state_ = ReceiveMicrosliceHeader;
	    next_receive_size_  = sizeof(RceMicrosliceHeader) - microslice_size_recvd_;
	    RECV_DEBUG(2) << "Incomplete header received for microslice " << microslices_recvd_
	                  << " size " << microslice_size_recvd_ << " next recv size is " << next_receive_size_;
	  }
	  break;

	case ReceiveMicroslicePayload:

		RECV_DEBUG(2) << "Got microslice payload length " << length;

		if (microslice_size_recvd_ == microslice_size_)
		{
			RECV_DEBUG(2) << "Complete payload received for microslice " << microslices_recvd_;
			microslices_recvd_++;
			microslice_size_recvd_ = 0;
			millislice_state_ = MillisliceIncomplete;
			next_receive_state_ = ReceiveMicrosliceHeader;
			next_receive_size_ = sizeof(RceMicrosliceHeader);
			current_header_ptr_ = (void*)((char*)current_header_ptr_ + microslice_size_);
		}
		else
		{
			RECV_DEBUG(2) << "Incomplete payload received for microslice " << microslices_recvd_
					      << " (got " << microslice_size_recvd_
					      << " expected " << microslice_size_ << " bytes)";
			millislice_state_ = MicrosliceIncomplete;
			next_receive_state_ = ReceiveMicroslicePayload;
			next_receive_size_ = microslice_size_ - microslice_size_recvd_;
		}
		break;

	default:

		// Should never happen - bug or data corruption
		DAQLogger::LogError(instance_name_) << "Fatal error after async_recv - unrecognised next receive state: " << next_receive_state_;
		return;
		break;
	}

	// Update millislice size received and write offset
	millislice_size_recvd_ += length;
	current_write_ptr_ = (void*)((char*)current_write_ptr_ + length);

	// If correct number of microslices have been received, flag millislice as complete
	if (microslices_recvd_ == number_of_microslices_per_millislice_)
	{
		RECV_DEBUG(1) << "Millislice " << millislices_recvd_
					  << " complete with " << microslices_recvd_
					  << " microslices, total size " << millislice_size_recvd_ << " bytes";
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

void lbne::RceDataReceiver::suspend_readout(bool await_restart)
{
	readout_suspended_.store(true);

	if (await_restart)
	{
		RECV_DEBUG(2) << "TpcRceReceiver::suspend_readout: awaiting restart or shutdown";
		while (suspend_readout_.load() && run_receiver_.load())
		{
			usleep(tick_period_usecs_);
		}
		RECV_DEBUG(2) << "TpcRceReceiver::suspend_readout: restart or shutdown detected, exiting wait loop";
	}

}

void lbne::RceDataReceiver::set_deadline(DeadlineIoObject io_object, unsigned int timeout_usecs)
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

void lbne::RceDataReceiver::check_deadline(void)
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
		deadline_.async_wait(boost::bind(&lbne::RceDataReceiver::check_deadline, this));
	}
	else
	{
		RECV_DEBUG(1) << "Deadline actor terminating";
	}
}
