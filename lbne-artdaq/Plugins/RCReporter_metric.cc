// RCReporter.h: RunControl Metric Plugin
// Author: John Freeman
// Last Modified: Apr-2-2015
//
// An implementation of artdaq's MetricPlugin for RunControl reporting

#include "artdaq/Plugins/MetricMacros.hh"
#include "fhiclcpp/ParameterSet.h"
#include "zmq.hpp"
#include "I3JSON.hh"

#include <fstream>
#include <ctime>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <memory>

namespace lbne {
  class RCReporter : public artdaq::MetricPlugin {

  public:
    RCReporter(fhicl::ParameterSet ps) : 
      artdaq::MetricPlugin(ps),
      runcontrol_socket_address_(ps.get<std::string>("runcontrol_socket_address", "tcp://192.168.100.1:5000")),
      stopped_(true)
    {

      try {
	context_.reset( new zmq::context_t(1) );
      } catch (const zmq::error_t& errt) {
	std::cerr << "Caught exception in zmq::context_t construction; rethrowing" << std::endl;
	throw errt;
      }
      
      try {
	socket_.reset( new zmq::socket_t( *context_, ZMQ_PUSH));
      } catch (const zmq::error_t& errt) {
	std::cerr << "Caught exception in zmq::socket_t construction; rethrowing" << std::endl;
	throw errt;
      }

      try {
	socket_->connect(runcontrol_socket_address_.c_str());
      } catch (const zmq::error_t& errt) {
	std::cerr << "Caught exception attempting 0MQ connection to " << \
	  runcontrol_socket_address_ << "; rethrowing" << std::endl;
	throw errt;
      }

      startMetrics();
    }

    ~RCReporter() {
      stopMetrics();
    }

    virtual std::string getLibName() { return "RCReporter"; }

    virtual void sendMetric(std::string name, std::string value, std::string unit ) 
    {

      if (!stopped_) {

      	std::ostringstream metric_msg_oss;
      	metric_msg_oss << name << ": " << value << " == " << unit << std::endl;

	std::time_t t = std::time(NULL);
	char timestr_cstyle[100];

	// JCF, 4/6/15 

	// The precision of the time of the metric reporting object is
	// only good to the nearest second; however, six trailing
	// zeroes have been added to satisfy formatting
	// requirements. As precision beyond the level of one second
	// is not needed and the fact that the fractional part of the
	// seconds value will always be six zeroes (and therefore
	// unlikely to be confused with actual microsecond-level
	// precision), this should not be too much of a problem.
	
	size_t chars_written = std::strftime(timestr_cstyle, sizeof(timestr_cstyle), 
					     "%Y-%m-%d %H:%M:%S.000000", std::gmtime(&t));
	
	if (chars_written == 0) {
	  throw cet::exception("Error in RCReporter metric: unable to write out the time to string");
	}

      	object_t json_msg;
      	json_msg["msg"] = metric_msg_oss.str().c_str();
      	json_msg["source"] = getLibName();
	json_msg["t"] = timestr_cstyle;

      	std::ostringstream json_msg_oss;
      	json_msg_oss << json_msg;

	zmq::message_t zmq_msg(json_msg_oss.str().length());

      	memcpy( zmq_msg.data(), json_msg_oss.str().c_str(), json_msg_oss.str().length());

	try {
	  socket_->send(zmq_msg, 0);
	} catch (const zmq::error_t& errt) {
	  std::cerr << "Caught exception attempting to send 0MQ message; rethrowing" << std::endl;
	  throw errt;
	}
      }
    }

    virtual void sendMetric(std::string name, int value, std::string unit ) 
    { 
      sendMetric(name, std::to_string(value), unit);
    }
    virtual void sendMetric(std::string name, double value, std::string unit ) 
    { 
      sendMetric(name, std::to_string(value), unit);
    }
    virtual void sendMetric(std::string name, float value, std::string unit ) 
    {
      sendMetric(name, std::to_string(value), unit);
    }
    virtual void sendMetric(std::string name, unsigned long int value, std::string unit ) 
    { 
      sendMetric(name, std::to_string(value), unit);
    }

    virtual void startMetrics() { stopped_ = false; }

    virtual void stopMetrics() {}

  private:

    std::string runcontrol_socket_address_; 

    bool stopped_;

    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> socket_;
  };

} //End namespace lbne

DEFINE_ARTDAQ_METRIC(lbne::RCReporter)
