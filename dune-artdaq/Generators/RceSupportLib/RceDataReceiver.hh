/*
 * RceDataReceiver.hh
 *
 *  Created on: May 16, 2014
 *      Author: tcn45
 */

#ifndef RCEDATARECEIVER_HH_
#define RCEDATARECEIVER_HH_

#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <ctime>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "dune-artdaq/Generators/RceSupportLib/SafeQueue.hh"
#include "dune-artdaq/Generators/RceSupportLib/RceRawBuffer.hh"
#include "messagefacility/MessageLogger/MessageLogger.h"

using boost::asio::ip::tcp;

namespace dune {

class RceDataReceiver {

public:
	RceDataReceiver(const std::string& instance_name, int debug_level,
			uint32_t tick_period_usecs, uint16_t udp_receive_port,
			uint16_t number_of_microslices_per_millislice,
			std::size_t max_buffer_attempts);
	virtual ~RceDataReceiver();

	void start();
	void stop();

	void commit_empty_buffer(RceRawBufferPtr& buffer);
	size_t empty_buffers_available(void);
	size_t filled_buffers_available(void);
	bool retrieve_filled_buffer(RceRawBufferPtr& buffer, unsigned int timeout_us=0);
	void release_empty_buffers(void);
	void release_filled_buffers(void);

        bool exception() const { return exception_.load(); }

private:

	enum DeadlineIoObject { None, Acceptor, DataSocket };

	void run_service(void);
	void do_accept(void);
	void do_read(void);
	void handle_received_data(std::size_t length);
	void suspend_readout(bool await_restart);

	void set_deadline(DeadlineIoObject io_object, unsigned int timeout_us);
	void check_deadline(void);
    void set_exception( bool exception ) { exception_.store( exception ); }

	std::string instance_name_;
	int debug_level_;

	boost::asio::io_service io_service_;
	tcp::acceptor           acceptor_;
	tcp::socket				accept_socket_;
	tcp::socket				data_socket_;

	boost::asio::deadline_timer deadline_;
	DeadlineIoObject deadline_io_object_;
	uint32_t tick_period_usecs_;

	uint16_t receive_port_;
	uint16_t number_of_microslices_per_millislice_;

	std::atomic<bool> run_receiver_;
	std::atomic<bool> suspend_readout_;
	std::atomic<bool> readout_suspended_;
        std::atomic<bool> exception_;

	int recv_socket_;
        size_t max_buffer_attempts_;

	std::unique_ptr<std::thread> receiver_thread_;

	SafeQueue<dune::RceRawBufferPtr> empty_buffer_queue_;
	SafeQueue<dune::RceRawBufferPtr> filled_buffer_queue_;
	RceRawBufferPtr current_raw_buffer_;
	void*           current_write_ptr_;
        void*           current_header_ptr_;

  enum NextReceiveState { ReceiveMicrosliceHeader, ReceiveMicroslicePayload, BlowOff };
	NextReceiveState next_receive_state_;
	size_t           next_receive_size_;

	enum MillisliceState { MillisliceEmpty, MillisliceIncomplete, MicrosliceIncomplete, MillisliceComplete };
	MillisliceState  millislice_state_;
	size_t           millislice_size_recvd_;
	uint16_t         microslices_recvd_;
	uint32_t         microslice_size_;
	size_t           microslice_size_recvd_;
	uint32_t         millislices_recvd_;

	bool     sequence_id_initialised_;
	uint32_t last_sequence_id_;

	std::chrono::high_resolution_clock::time_point start_time_;

};

} /* namespace dune */

#endif /* RCEDATARECEIVER_HH_ */
