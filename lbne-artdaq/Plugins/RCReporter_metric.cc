// RCReporter_metric.cc: RunControl Metric Plugin
// Author: John Freeman
//
// An implementation of artdaq's MetricPlugin for RunControl reporting

#include "lbne-artdaq/RCConnection/RCConnection.hh"
#include "lbne-artdaq/RCConnection/I3JSON.hh"

#include "artdaq/Plugins/MetricMacros.hh"
#include "fhiclcpp/ParameterSet.h"


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

	RCConnection::Get().Send( getLibName(), metric_msg_oss.str(), "Info");
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
