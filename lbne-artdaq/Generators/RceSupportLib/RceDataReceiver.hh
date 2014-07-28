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
#include <chrono>
#include <ctime>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include "lbne-artdaq/Generators/RceSupportLib/SafeQueue.hh"
#include "lbne-artdaq/Generators/RceSupportLib/RceRawBuffer.hh"

using boost::asio::ip::tcp;

namespace lbne {

class RceDataReceiver {

public:
	RceDataReceiver(int debug_level, uint16_t udp_receive_port, size_t raw_buffer_size, uint16_t number_of_microslices_per_millislice);
	virtual ~RceDataReceiver();

	void start();
	void stop();

	void commit_empty_buffer(RceRawBufferPtr& buffer);
	size_t empty_buffers_available(void);
	size_t filled_buffers_available(void);
	bool retrieve_filled_buffer(RceRawBufferPtr& buffer, unsigned int timeout_millisecs=0);
	void release_empty_buffers(void);


private:

	enum DeadlineIoObject { None, Acceptor, DataSocket };

	void run_service(void);
	void do_accept(void);
	void do_read(void);
	void handle_received_data(std::size_t length);

	void set_deadline(DeadlineIoObject io_object, unsigned int timeout_ms);
	void check_deadline(void);


	int debug_level_;

	boost::asio::io_service io_service_;
	tcp::acceptor           acceptor_;
	tcp::socket				accept_socket_;
	tcp::socket				data_socket_;

	boost::asio::deadline_timer deadline_;
	DeadlineIoObject deadline_io_object_;

	uint16_t receive_port_;
	const size_t raw_buffer_size_;
	uint16_t number_of_microslices_per_millislice_;

	bool run_receiver_;
	int recv_socket_;

	std::unique_ptr<std::thread> receiver_thread_;

	SafeQueue<lbne::RceRawBufferPtr> empty_buffer_queue_;
	SafeQueue<lbne::RceRawBufferPtr> filled_buffer_queue_;
	RceRawBufferPtr current_raw_buffer_;
	void*           current_write_ptr_;

	enum NextReceiveState { ReceiveMicrosliceHeader, ReceiveMicroslicePayload };
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

} /* namespace lbne */

#endif /* RCEDATARECEIVER_HH_ */
