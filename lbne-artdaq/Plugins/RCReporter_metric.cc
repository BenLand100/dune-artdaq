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
#include <stdexcept>
//#include <limits>

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

    // JCF, Oct-30-2015

    // Erik Blaufuss has requested that metric data be sent to
    // RunControl in the form of its original data type, rather than
    // as a string, which had previously been the case. So, e.g., send
    // RunControl the integer 7 rather than the ASCII character '7
    // followed by a '\0' (i.e., the string "7")

    // The upshot is, I now basically want identical overloads of all
    // the sendMetric_ pure virtual functions declared in
    // artdaq::MetricPlugin. Usually this would suggest
    // templatization, however, you can't templatize virtual
    // functions, so instead I've created the macro
    // "GENERATE_SENDMETRIC_FUNCTION" in order to avoid
    // cut-and-pasting the same code for every value type

#define GENERATE_SENDMETRIC_FUNCTION(TYPE)				\
									\
    virtual void sendMetric_(std::string name, TYPE value, std::string unit) \
    {									\
      if (!stopped_) {							\
									\
	RCConnection::Get().SendMetric( getLibName(), name, value);	\
      }									\
    }

    GENERATE_SENDMETRIC_FUNCTION(std::string)
    GENERATE_SENDMETRIC_FUNCTION(int)
    GENERATE_SENDMETRIC_FUNCTION(double)
    GENERATE_SENDMETRIC_FUNCTION(float)

    // JCF, Oct-30-2015

    // It appears boost::detail::variant::make_initializer_node::apply
    // can't handle an "unsigned long int", so instead of sending the
    // value to RunControl I try to recast it as an int; if this
    // doesn't work, I throw a runtime_error.

    virtual void sendMetric_(std::string name, unsigned long int value, std::string unit)
    {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"

      auto origval = value;
	
      int newval = static_cast<int>( value );
      
      if (newval == origval) {
	sendMetric_(name, newval, unit);
      } else {
	throw std::runtime_error("Error in RCReporter: can't send an unsigned long int to RunControl; please contact John Freeman");
      }
#pragma GCC diagnostic pop
    }
    
#undef GENERATE_SENDMETRIC_FUNCTION

#pragma GCC diagnostic pop

    virtual void startMetrics_() { stopped_ = false; }

    virtual void stopMetrics_() {}

  private:

    bool stopped_;
  };

} //End namespace lbne

DEFINE_ARTDAQ_METRIC(lbne::RCReporter)
