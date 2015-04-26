
#include "RCConnection.hh"

#include "zmq.hpp"

#include <string>
#include <iostream>

namespace lbne {

  RCConnection::RCConnection() :
    runcontrol_socket_address_("tcp://192.168.100.1:5000"),
    connection_opened_(false)
  {
  }

  void RCConnection::Send(const std::string msg) {
   
    if (!connection_opened_) {
      InitConnection();
    }
    
    zmq::message_t zmq_msg(msg.length());

    memcpy( zmq_msg.data(), msg.c_str(), msg.length());

    try {
      socket_->send(zmq_msg, 0);
    } catch (const zmq::error_t& errt) {
      std::cerr << "Error in RCConnection: Caught exception attempting to send " << 
	"0MQ message \"" << msg << "\" to RunControl; rethrowing" << std::endl;
      throw errt;
    }
  }

  void RCConnection::InitConnection() {

    // Will probably need to lock this...

    try {
      context_.reset( new zmq::context_t(1) );
    } catch (const zmq::error_t& errt) {
      std::cerr << "Error in RCConnection: caught exception in zmq::context_t construction; rethrowing" << 
	std::endl;
      throw errt;
    }

    try {
      socket_.reset( new zmq::socket_t( *context_, ZMQ_PUSH));
    } catch (const zmq::error_t& errt) {
      std::cerr << "Error in RCConnection: caught exception in zmq::socket_t construction; rethrowing" << 
	std::endl;			
      throw errt;
    }

    try {
      socket_->connect(runcontrol_socket_address_.c_str());
    } catch (const zmq::error_t& errt) {
      std::cerr << "Error in RCConnection: caught exception attempting 0MQ connection to " << \
	runcontrol_socket_address_ << "; rethrowing" << std::endl;
      throw errt;
    }

    connection_opened_ = true;
  }

} // End namespace lbne
