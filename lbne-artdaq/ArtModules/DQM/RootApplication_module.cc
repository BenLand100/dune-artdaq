////////////////////////////////////////////////////////////////////////
// Class:       RootApplication
// Module Type: analyzer
// File:        RootApplication_module.cc
//
// Generated at Sun Dec  2 12:23:06 2012 by Alessandro Razeto & Nicola Rossi using artmod
// from art v1_02_04.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Core/ModuleMacros.h"

#include <iostream>
#include <future>
#include <TApplication.h>
#include <TSystem.h>
#include <unistd.h>

namespace lbne {
  class RootApplication : public art::EDAnalyzer {
    public:
      explicit RootApplication (fhicl::ParameterSet const & p);
      virtual ~RootApplication ();

      void beginJob() override;
      void analyze (art::Event const & e) override;
      void endJob() override;

    private:
      std::unique_ptr<TApplication> app_;
      bool force_new_;
      bool dont_quit_;
  };
}

lbne::RootApplication::RootApplication (fhicl::ParameterSet const &ps): art::EDAnalyzer(ps), force_new_(ps.get<bool> ("force_new", true)), dont_quit_(ps.get<bool> ("dont_quit", false)) {}

lbne::RootApplication::~RootApplication () { }

void lbne::RootApplication::analyze (art::Event const &) {
  gSystem->ProcessEvents();
}

void lbne::RootApplication::beginJob () { 
  if (!gApplication || force_new_) {
    int tmp_argc(0);
    app_ = std::unique_ptr<TApplication>(new TApplication("noapplication", &tmp_argc, 0));
  }
}

void lbne::RootApplication::endJob() {
  if (dont_quit_) app_->Run (true);
}

DEFINE_ART_MODULE(lbne::RootApplication)
