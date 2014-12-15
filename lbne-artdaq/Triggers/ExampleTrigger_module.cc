////////////////////////////////////////////////////////////////////////
// Class:       ExampleTrigger
// Module Type: filter
// File:        ExampleTrigger_module.cc
//
// Generated at Fri Dec  5 10:21:00 2014 by Jonathan Davies using artmod
// from cetpkgsupport v1_07_01.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDFilter.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <memory>

#include <iostream>

namespace trig {
  class ExampleTrigger;
}

class trig::ExampleTrigger : public art::EDFilter {
public:
  explicit ExampleTrigger(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  ExampleTrigger(ExampleTrigger const &) = delete;
  ExampleTrigger(ExampleTrigger &&) = delete;
  ExampleTrigger & operator = (ExampleTrigger const &) = delete;
  ExampleTrigger & operator = (ExampleTrigger &&) = delete;

  // Required functions.
  bool filter(art::Event & e) override;
  void printParams();

private:

  std::string _hitsModuleLabel;           ///< label of module making the list of hits
  std::string _hitsInstanceLabel;         ///< instance label making the list of hits

};


trig::ExampleTrigger::ExampleTrigger(fhicl::ParameterSet const & p)
// Initialize member data here.
{
  _hitsModuleLabel = p.get<std::string>  ("HitModuleLabel", "DefaultHitModuleLabel");
  _hitsInstanceLabel = p.get<std::string>  ("HitInstanceLabel", "DefaultHitInstanceLabel");

  printParams();

}

void trig::ExampleTrigger::printParams(){
  
  for(int i=0;i<80;i++) std::cout << "=";
  std::cout << std::endl;

  std::cout << "_hitsModuleLabel: " << _hitsModuleLabel << std::endl;
  std::cout << "_hitsInstanceLabel: " << _hitsInstanceLabel << std::endl;


  for(int i=0;i<80;i++) std::cout << "=";
  std::cout << std::endl;


}

bool trig::ExampleTrigger::filter(art::Event & e)
{
  // Implementation of required member function here.
  auto eventID = e.id();
  
  std::cout << "eventID: " << eventID << std::endl;

  return 1;
}

DEFINE_ART_MODULE(trig::ExampleTrigger)
