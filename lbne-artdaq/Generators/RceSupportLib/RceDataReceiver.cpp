/*
 * RceDataReceiver.cpp
 *
 *  Created on: May 16, 2014
 *      Author: tcn45
 */

#include "RceDataReceiver.hh"

#include <iostream>

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

lbne::RceDataReceiver::RceDataReceiver(uint16_t udp_receive_port, size_t raw_buffer_size) :
	udp_receive_port_(udp_receive_port),
	raw_buffer_size_(raw_buffer_size),
	run_receiver_(false),
	recv_socket_(0)
{
	std::cout << "lbne::RceDataReceiver constructor" << std::endl;
}

lbne::RceDataReceiver::~RceDataReceiver()
{
	std::cout << "lbne::RceDataReceiver destructor" << std::endl;
}

void lbne::RceDataReceiver::start(void)
{
	std::cout << "lbne::RceDataReceiver::start called" << std::endl;

	// Open UDP socket to receive data on

	recv_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
	if (recv_socket_ < 0)
	{
		std::cout << "lbne::RceDataReceiver::start : UDP socket creation failed" << std::endl;
		// TODO throw exception or create error return here
	}

	// Bind the socket to the appropriate port

	struct sockaddr_in server_sock;
	memset(&server_sock, 0, sizeof(server_sock));
	server_sock.sin_family = AF_INET;
	server_sock.sin_port = htons(udp_receive_port_);
	server_sock.sin_addr.s_addr = htonl(INADDR_ANY);

	std::cout << "Binding UDP socket to port " << udp_receive_port_ << std::endl;

	if (bind(recv_socket_, (struct sockaddr*)&server_sock, sizeof(struct sockaddr_in)) == -1)
	{
		std::cout << "lbne::RceDataReceiver::start : Failed to bind UDP socket: " << strerror(errno)<< std::endl;
		// TODO throw exception or create error return here
	}

	run_receiver_ = true;
	udp_receiver_thread_ = std::unique_ptr<std::thread>(new std::thread(&lbne::RceDataReceiver::receiverLoop, this));

}

void lbne::RceDataReceiver::stop(void)
{
	std::cout << "lbne::RceDataReceiver:::stop called" << std::endl;
	run_receiver_ = false;
	udp_receiver_thread_->join();
	std::cout << "lbne::RceDataReceiver::stop receiver thread joined OK" << std::endl;

	close(recv_socket_);
	recv_socket_ = 0;
}

void lbne::RceDataReceiver::commitEmptyBuffer(RceRawBufferPtr& buffer)
{
	empty_buffer_queue_.push(std::move(buffer));
}

bool lbne::RceDataReceiver::retrieveFilledBuffer(RceRawBufferPtr& buffer, unsigned int timeout_millisecs)
{
	bool buffer_available = true;

	// If timeout is specified, try to pop the buffer from the queue and time out, otherwise
	// block waiting for the queue to contain a buffer
	if (timeout_millisecs == 0)
	{
		buffer = filled_buffer_queue_.pop();
	}
	else
	{
		buffer_available = filled_buffer_queue_.try_pop(buffer, std::chrono::milliseconds(timeout_millisecs));
	}

	return buffer_available;
}

void lbne::RceDataReceiver::receiverLoop(void)
{

	std::cout << "lbne::RceDataReceiver::receiverLoop starting" << std::endl;

	while (run_receiver_)
	{

		RceRawBufferPtr raw_buffer;
		bool buffer_available = empty_buffer_queue_.try_pop(raw_buffer, std::chrono::milliseconds(1000));

		if (!buffer_available) {
			std::cout << "lbne::RceDataReceiver::receiverLoop no buffers available on commit queue" <<std::endl;;
			break;
		}
		else
		{
			std::cout << "Receiving data into raw buffer at address " << (void*) (raw_buffer->dataPtr()) << std::endl;
		}

		struct sockaddr src_addr;
		socklen_t src_addr_len = sizeof(src_addr);
		ssize_t recv_size = recvfrom(recv_socket_, raw_buffer->dataPtr(), raw_buffer->size(), 0, &src_addr, &src_addr_len);

		if (recv_size == -1)
		{
			std::cout << "lbne::RceDataReceiver::receiverLoop receive failed: " << recv_size << std::endl;
		}
		else
		{
			struct sockaddr_in* sa = reinterpret_cast<struct sockaddr_in*>(&src_addr);
			std::cout << "Received " << recv_size << " bytes from address " << inet_ntoa(sa->sin_addr) << std::endl;
			raw_buffer->setSize(recv_size);
			filled_buffer_queue_.push(std::move(raw_buffer));
		}
	}

	std::cout << "lbne::RceDataReceiver::receiverLoop terminating" << std::endl;

}
