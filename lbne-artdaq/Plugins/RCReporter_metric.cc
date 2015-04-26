// RCReporter_metric.cc: RunControl Metric Plugin
// Author: John Freeman
//
// An implementation of artdaq's MetricPlugin for RunControl reporting

#include "lbne-artdaq/RCConnection/RCConnection.hh"

#include "artdaq/Plugins/MetricMacros.hh"
#include "fhiclcpp/ParameterSet.h"
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
      stopped_(true)
    {
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

	RCConnection& myconnection = RCConnection::Get();
	myconnection.Send( json_msg_oss.str());
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

    bool stopped_;
  };

} //End namespace lbne

DEFINE_ARTDAQ_METRIC(lbne::RCReporter)
