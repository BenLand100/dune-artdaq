////////////////////////////////////////////////////////////////////////
// Class:       BasicMetrics
// Module Type: analyzer
// File:        BasicMetrics_module.cc
// Description: Prints out number of events which have passed through
// it + total size of the fragments of a given set of types (possibly
// all types)
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"

#include "canvas/Utilities/Exception.h"
#include "artdaq-core/Data/Fragments.hh"
#include "artdaq/DAQrate/MetricManager.hh"

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>

namespace dune {
  class BasicMetrics;
}

class dune::BasicMetrics : public art::EDAnalyzer {
public:
  explicit BasicMetrics(fhicl::ParameterSet const & pset);
  virtual ~BasicMetrics();

  virtual void analyze(art::Event const & evt);

private:
  std::string raw_data_label_;
  std::vector<std::string> frag_types_;

  std::size_t events_processed_;
  std::size_t bytes_processed_;

  //  std::string process_label_;

  artdaq::MetricManager metricMan_;
};


dune::BasicMetrics::BasicMetrics(fhicl::ParameterSet const & pset)
    : EDAnalyzer(pset),
      raw_data_label_(pset.get<std::string>("raw_data_label")),
      frag_types_(pset.get<std::vector<std::string>>("frag_types")),
      events_processed_(0),
      bytes_processed_(0)//,
      //      process_label_(pset.get<std::string>("process_label"))
{
  // pull out the relevant parts of the ParameterSet                                                       

  fhicl::ParameterSet metric_pset;
           
  try {
    metric_pset = pset.get<fhicl::ParameterSet>("metrics");
    metricMan_.initialize(metric_pset, "BasicMetrics.");
    metricMan_.do_start();
  } catch (...) {
    mf::LogInfo("BasicMetrics")
      << "Unable to find the metrics parameters in the metrics "
      << "ParameterSet: \"" + pset.to_string() + "\".";
  }
}

dune::BasicMetrics::~BasicMetrics()
{
   metricMan_.do_stop();
   metricMan_.shutdown();
}

void dune::BasicMetrics::analyze(art::Event const & evt)
{

  events_processed_++;

  for (const auto frag_type : frag_types_) {

    art::Handle<artdaq::Fragments> raw;
    evt.getByLabel(raw_data_label_, frag_type, raw);

    if (raw.isValid()) {

      if (events_processed_ % 100 == 0) {
	mf::LogInfo("BasicMetrics") << "Processed " << events_processed_ << " events, " <<
	  "have " << raw->size() << " fragments of type " << frag_type;
      }

      for (size_t idx = 0; idx < raw->size(); ++idx) {
	const auto& frag((*raw)[idx]);
	bytes_processed_ += frag.sizeBytes();
      }
    }

    std::cout << std::endl;
  }

  if (events_processed_ % 100 == 0) {
    mf::LogInfo("BasicMetrics") << "Processed " << events_processed_ << " events; total size is " <<
      bytes_processed_/(1024.0*1024.0) << " megabytes";

    //    metricMan_.sendMetric("Events processed",events_processed_, process_label_, 0);
    //    metricMan_.sendMetric("MB processed",bytes_processed_/(1024.0*1024.0), process_label_, 0);
    metricMan_.sendMetric("Events processed",events_processed_, "Aggregator", 0);
    metricMan_.sendMetric("MB processed",bytes_processed_/(1024.0*1024.0), "Aggregator", 0);
  }

}

DEFINE_ART_MODULE(dune::BasicMetrics)
