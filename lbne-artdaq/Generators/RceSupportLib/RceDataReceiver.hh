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
#include "lbne-artdaq/Generators/RceSupportLib/SafeQueue.hh"
#include "lbne-artdaq/Generators/RceSupportLib/RceRawBuffer.hh"

namespace lbne {

class RceDataReceiver {

public:
	RceDataReceiver(uint16_t udp_receive_port, size_t raw_buffer_size);
	virtual ~RceDataReceiver();

	void start();
	void stop();

	static constexpr unsigned int bufferQueueCapacity = 32;

	void commitEmptyBuffer(RceRawBufferPtr& buffer);
	bool retrieveFilledBuffer(RceRawBufferPtr& buffer, unsigned int timeout_millisecs=0);

private:

	void receiverLoop();

	uint16_t udp_receive_port_;
	const size_t raw_buffer_size_;
	bool run_receiver_;
	int recv_socket_;

	std::unique_ptr<std::thread> udp_receiver_thread_;

	SafeQueue<lbne::RceRawBufferPtr> empty_buffer_queue_;
	SafeQueue<lbne::RceRawBufferPtr> filled_buffer_queue_;
};

} /* namespace lbne */

#endif /* RCEDATARECEIVER_HH_ */
