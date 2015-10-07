// RCReporter_metric.cc: RunControl Metric Plugin
// Author: John Freeman
//
// An implementation of artdaq-utilities's MetricPlugin for RunControl reporting

#include "lbne-artdaq/RCConnection/RCConnection.hh"
#include "lbne-artdaq/RCConnection/I3JSON.hh"

#include "artdaq-utilities/Plugins/MetricMacros.hh"
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

    // JCF, 6/11/15

    // For now, at least, the "unit" argument to sendMetric (whose
    // signature is defined in artdaq) will be ignored (though I may
    // later add an additional field including the unit), so I'll
    // locally turn off the ensuing warning

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"


    virtual void sendMetric_(std::string name, std::string value, std::string unit) 
    {
      if (!stopped_) {

	RCConnection::Get().SendMetric( getLibName(), name, value);
      }
    }
#pragma GCC diagnostic pop

    virtual void sendMetric_(std::string name, int value, std::string unit ) 
    { 
      sendMetric_(name, std::to_string(value), unit);
    }
    virtual void sendMetric_(std::string name, double value, std::string unit ) 
    { 
      sendMetric_(name, std::to_string(value), unit);
    }
    virtual void sendMetric_(std::string name, float value, std::string unit ) 
    {
      sendMetric_(name, std::to_string(value), unit);
    }
    virtual void sendMetric_(std::string name, unsigned long int value, std::string unit ) 
    { 
      sendMetric_(name, std::to_string(value), unit);
    }

    virtual void startMetrics_() { stopped_ = false; }

    virtual void stopMetrics_() {}

  private:

    bool stopped_;
  };

} //End namespace lbne

DEFINE_ARTDAQ_METRIC(lbne::RCReporter)
