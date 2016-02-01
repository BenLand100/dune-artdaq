
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "RCConnection.hh"
#include "I3JSON.hh"

#include "zmq.hpp"

#include <string>
#include <iostream>
#include <algorithm>

namespace lbne {

  RCConnection::RCConnection() :
    runcontrol_socket_address_("tcp://10.226.8.24:5000"),
    connection_opened_(false)
  {
  }
  
  void RCConnection::Send(const std::string& json_msg) {

    // 0MQ sockets are not thread safe, so make sure only one thread
    // is calling the "Send" function (and thereby using the
    // zmq::socket_t object) at a time

    std::unique_lock<std::mutex> lock(mutex_);
   
    if (!connection_opened_) {
      InitConnection();
    }
    
    zmq::message_t zmq_msg(json_msg.length());

    memcpy( zmq_msg.data(), json_msg.c_str(), json_msg.length());

    try {
      socket_->send(zmq_msg, 0);
    } catch (const zmq::error_t& errt) {
      mf::LogWarning("RCConnection") << "Caught exception attempting to send " << 
	"JSON object via 0MQ to RunControl; rethrowing. Object was \"" << json_msg << "\"";
      throw errt;
    }
  }

  void RCConnection::SendMessage(const std::string& service, const std::string& msg, 
				 const std::string& severity) {
    std::string json_msg = MsgToRCJSON(service, msg, severity);
    Send(json_msg);
  }


  void RCConnection::InitConnection() {

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
