/*
 * RceDataReceiver.cpp - RCE data recevier classed based on the Boost asynchronous IO network library
 *
 *  Created on: May 16, 2014
 *      Author: Tim Nicholls, STFC Application Engineering Group
 *  Modified:  February 13, 2017
 *      by Matt Graham for protoDUNE 
 */

#include "RceDataReceiver.hh"
#include "dune-artdaq/DAQLogger/DAQLogger.hh"

#include <iostream>
#include <unistd.h>

struct RceMicrosliceHeader
{
  uint32_t microslice_size;
  uint32_t rx_sequence_id;
  uint32_t tx_sequence_id;

  uint32_t type_id;
};

struct BlowOffData{
    uint32_t bo;
};

#define RECV_DEBUG(level) if (level <= debug_level_) DAQLogger::LogDebug(instance_name_)
const uint   safeWord=0x708b309e;

dune::RceDataReceiver::RceDataReceiver(const std::string& instance_name, int debug_level, uint32_t tick_period_usecs,
				       uint16_t receive_port, uint16_t number_of_microslices_per_millislice, std::size_t max_buffer_attempts) :
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
	exception_(false),
	recv_socket_(0),
	max_buffer_attempts_(max_buffer_attempts)
{
	RECV_DEBUG(1) << "dune::RceDataReceiver constructor";

	// Initialise and start the persistent deadline actor that handles socket operation
	// timeouts. Timeout is set to zero (no timeout) to start
	this->set_deadline(None, 0);
	this->check_deadline();

	// Start asynchronous accept handler
	this->do_accept();

	// Launch thread to run receiver IO service
	receiver_thread_ = std::unique_ptr<std::thread>(new std::thread(&dune::RceDataReceiver::run_service, this));

}

dune::RceDataReceiver::~RceDataReceiver()
{
	RECV_DEBUG(1) << "dune::RceDataReceiver destructor";

	// Flag receiver as no longer running
	run_receiver_.store(false);

	// Cancel any currently pending IO object deadlines and terminate timer
	deadline_.expires_at(boost::asio::deadline_timer::traits_type::now());

	// Wait for thread running receiver IO service to terminate
	receiver_thread_->join();

	RECV_DEBUG(1) << "dune::RceDataReceiver destructor: receiver thread joined OK";

}

void dune::RceDataReceiver::start(void)
{
	RECV_DEBUG(1) << "dune::RceDataReceiver::start called";

	start_time_ = std::chrono::high_resolution_clock::now();

	// If the data receive socket is open, flush any stale data off it
	if (deadline_io_object_ == DataSocket)
	{

		std::size_t flush_length = 0;

		while (data_socket_.available() > 0)
		{
			boost::array<char, 65536> buf;
			boost::system::error_code ec;

			size_t len = data_socket_.read_some(boost::asio::buffer(buf), ec);
			flush_length += len;
			RECV_DEBUG(3) << "Flushed: " << len << " total: " << flush_length
					<< " available: " << data_socket_.available();

			if (ec == boost::asio::error::eof)
			{
				DAQLogger::LogInfo(instance_name_) << "dune::RceDataReceiver:start: client closed data socket connection during flush operation";
				break;
			}
			else if (ec)
			{
				DAQLogger::LogWarning(instance_name_) << "dune::RceDataReceiver::start: got unexpected socket error flushing data socket: " << ec;
				break;
			}
		}
		if (flush_length > 0)
		{
		  // JCF, Feb-4-2016

		  // I've promoted this "stale data" message to a
		  // warning since there appears to be a 1-1
		  // correspondence between this condition being met
		  // and a subsequent "Error on asynchronous read"
		  // error later in the run

			DAQLogger::LogWarning(instance_name_) << "dune::RceDataReceiver::start: flushed " << flush_length << " bytes stale data off open socket";
		}
		else
		{
			DAQLogger::LogDebug(instance_name_) << "dune::RceDataReceiver::start: no stale data to flush off socket";
		}
	}

	// Clean up current buffer state of any partially-completed readouts in previous runs
	if (current_raw_buffer_ != nullptr)
	{
		RECV_DEBUG(0) << "dune::RceDataReceiver::start: dropping unused or partially filled buffer containing "
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

void dune::RceDataReceiver::stop(void)
{
	RECV_DEBUG(1) << "dune::RceDataReceiver::stop called";

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

	RECV_DEBUG(0) << "dune::RceDataRecevier::stop : last sequence id was " << last_sequence_id_;
	RECV_DEBUG(0) << "dune::RceDataReceiver::stop : received " << millislices_recvd_ << " millislices in "
			      << elapsed_secs << " seconds, rate "
			      << rate << " Hz";

}

void dune::RceDataReceiver::commit_empty_buffer(RceRawBufferPtr& buffer)
{
	empty_buffer_queue_.push(std::move(buffer));
}

size_t dune::RceDataReceiver::empty_buffers_available(void)
{
	return empty_buffer_queue_.size();
}

size_t dune::RceDataReceiver::filled_buffers_available(void)
{
	return filled_buffer_queue_.size();
}

bool dune::RceDataReceiver::retrieve_filled_buffer(RceRawBufferPtr& buffer, unsigned int timeout_us)
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

void dune::RceDataReceiver::release_empty_buffers(void)
{

	while (empty_buffer_queue_.size() > 0)
	{
		empty_buffer_queue_.pop();
	}
}

void dune::RceDataReceiver::release_filled_buffers(void)
{
	while (filled_buffer_queue_.size() > 0)
	{
		filled_buffer_queue_.pop();
	}
}

void dune::RceDataReceiver::run_service(void)
{
	RECV_DEBUG(1) << "dune::RceDataReceiver::run_service starting";

	io_service_.run();

	RECV_DEBUG(1) << "dune::RceDataReceiver::run_service stopping";
}

void dune::RceDataReceiver::do_accept(void)
{
	// Suspend readout if stop has been called
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

void dune::RceDataReceiver::do_read(void)
{

	// Suspend readout if stop has been called
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
		const unsigned int buffer_retry_report_interval = 10;
		bool buffer_available;

		do
		{
			buffer_available = empty_buffer_queue_.try_pop(current_raw_buffer_, std::chrono::milliseconds(100));
			if (!buffer_available)
			{
				if ((buffer_retries > 0) && ((buffer_retries % buffer_retry_report_interval) == 0)) {
					DAQLogger::LogWarning(instance_name_) << "dune::RceDataReceiver::receiverLoop: no buffers available on commit queue due to more data entering the RceDataReceiver than leaving it, retrying ("
							<< buffer_retries << " attempts so far)";
				}
				buffer_retries++;

				if (buffer_retries > max_buffer_attempts_)
				{
					if (!suspend_readout_.load())
					{
						if (!exception_.load() )
						{
							try {
								DAQLogger::LogError(instance_name_) << "dune::RceDataReceiver::receiverLoop: too many buffer retries, signalling an exception";
							} catch (...) {
								set_exception(true);
							}
						}
					}
				}
			} else {
				if (buffer_retries > buffer_retry_report_interval) {
					RECV_DEBUG(1) << "dune::RceDataReceiver::receiverLoop: obtained new buffer after " << buffer_retries << " retries";
				}
			}

			// Check if readout is being suspended/terminated and handle accordingly
			if (suspend_readout_.load())
			{
				DAQLogger::LogInfo(instance_name_) << "Suspending readout during buffer retry loop";
				this->suspend_readout(true);

				// When we come out of suspended at this point, the main thread will have invalidated the current buffer, so set
				// buffer_available to false so that we get a fresh one for the next read
				buffer_available = false;
			}

			// Terminate receiver read loop if required
			if (!run_receiver_.load())
			{
				RECV_DEBUG(1) << "Terminating receiver thread in buffer retry loop";
				return;
			}

		} while (!buffer_available);

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

			// TCN, Dec-04-15

			// Note this else clause is now technically redundant as the buffer retry loop will only exit either
			// with a valid buffer or because the receiver is being shut down. I have left the code in for now
			// since it demonstrates how to swallow the exception and signal at run end, which may become
			// necessary again

		  try {
		    DAQLogger::LogError(instance_name_) << "Failed to obtain new raw buffer for millislice, terminating receiver loop";
		  } catch (...) {

		    // JCF, Oct-24-15

		    // Swallow the exception... don't want to bring
		    // down the DAQ if the above error message throws
		    // an exception during the stop transition, as
		    // this means the output *.root file is in danger
		    // of not properly being closed

		    // JCF, Nov-3-2015:

		    // However: since we can't obtain a new raw buffer
		    // for the millislice at this point, then if we're
		    // in the running state, we'll set the exception_
		    // flag, so getNext will know to stop the run

		    if (! suspend_readout_.load()) {
		      DAQLogger::LogInfo(instance_name_) << "Setting exception to true; suspend_readout is false";
		      set_exception(true);
		    }
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

				  try {
				    DAQLogger::LogError(instance_name_) << "Got error on aysnchronous read: " << ec << "\n"
									<< "RECV: state " << (unsigned int)next_receive_state_
									<< " mslice state " << (unsigned int)millislice_state_
									<< " uslice " << microslices_recvd_
									<< " uslice size " << microslice_size_recvd_
									<< " mslice size " << millislice_size_recvd_
									<< " addr " << current_write_ptr_
									<< " next recv size " << next_receive_size_;
				  } catch (...) {
				    set_exception(true);
				  }

				}

			}
		}
	);
}

void dune::RceDataReceiver::handle_received_data(std::size_t length)
{

	RceMicrosliceHeader* header;
	uint32_t sequence_id;

	uint32_t thisWord;
	microslice_size_recvd_ += length;

	switch (next_receive_state_)
	{
	case ReceiveMicrosliceHeader:

	  if (microslice_size_recvd_ == sizeof(RceMicrosliceHeader))
	  {
	    // Capture the microslice length and sequence ID from the header
	    header = reinterpret_cast<RceMicrosliceHeader*>(current_header_ptr_);

	    microslice_size_ = header->microslice_size;
	    sequence_id = header->rx_sequence_id;

	    RECV_DEBUG(2) << "Got header for microslice with size " << microslice_size_ << " sequence ID " << sequence_id;

	    // Validate the sequence ID - should be incrementing monotonically
	    if (sequence_id_initialised_ && (sequence_id != last_sequence_id_+1))
	    {
	      DAQLogger::LogWarning(instance_name_) << "Got mismatch in microslice sequence IDs! Got " << sequence_id << " expected " << last_sequence_id_+1;
	      DAQLogger::LogWarning(instance_name_) << "Receive length at mismatch : " << length;
	      DAQLogger::LogWarning(instance_name_) << "Microslice header at mismatch :"
					     << " size : 0x"         << std::hex << header->microslice_size << std::dec
						    << " rx_sequence ID : 0x" << std::hex << header->rx_sequence_id     << std::dec
						    << " tx_sequence ID : 0x" << std::hex << header->tx_sequence_id     << std::dec
						    << " type ID : 0x"      << std::hex << header->type_id         << std::dec;
		//TODO handle error cleanly here
	      //blow off data until we get to safeword
	      millislice_state_=MicrosliceIncomplete;
	      next_receive_state_ = BlowOff;
	      next_receive_size_ = sizeof(uint32_t);
	      last_sequence_id_++;
	      sequence_id_initialised_ = true;
	      break;

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

	case BlowOff:
	  //something went wrong with the header...try to loop through the data until we get the safeword
	  thisWord=*(uint32_t*)(current_write_ptr_);
	  if(thisWord==safeWord) {
	    DAQLogger::LogWarning(instance_name_) << "Found the safeword! after this many bytes " <<  microslice_size_recvd_;
	    microslices_recvd_++;
	    current_header_ptr_ = (void*)((char*)current_header_ptr_ + microslice_size_recvd_);
	    microslice_size_recvd_ = 0;
	    millislice_state_ = MillisliceIncomplete;
	    next_receive_state_ = ReceiveMicrosliceHeader;
	    next_receive_size_ = sizeof(RceMicrosliceHeader);
	  } else {
	    // DAQLogger::LogWarning(instance_name_) << "Blowing off! Word is this: 0x" << std::hex <<  thisWord<<std::dec<<"  after "<<microslice_size_recvd_<<" words...safeWord = "<<std::hex <<safeWord<<std::dec;
	    millislice_state_=MicrosliceIncomplete;
	    next_receive_state_ = BlowOff;
	    next_receive_size_ = sizeof(uint32_t);
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

void dune::RceDataReceiver::suspend_readout(bool await_restart)
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

void dune::RceDataReceiver::set_deadline(DeadlineIoObject io_object, unsigned int timeout_usecs)
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

void dune::RceDataReceiver::check_deadline(void)
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
		deadline_.async_wait(boost::bind(&dune::RceDataReceiver::check_deadline, this));
	}
	else
	{
		RECV_DEBUG(1) << "Deadline actor terminating";
	}
}
