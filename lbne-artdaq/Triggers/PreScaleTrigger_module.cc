////////////////////////////////////////////////////////////////////////
// Class:       PreScaleTrigger
// Module Type: filter
// File:        PreScaleTrigger_module.cc
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
#include <chrono>
#include <random>
#include <string>
#include <iostream>
#include <iomanip>

namespace trig {
  class PreScaleTrigger;
}

class trig::PreScaleTrigger : public art::EDFilter {
public:
  explicit PreScaleTrigger(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  PreScaleTrigger(PreScaleTrigger const &) = delete;
  PreScaleTrigger(PreScaleTrigger &&) = delete;
  PreScaleTrigger & operator = (PreScaleTrigger const &) = delete;
  PreScaleTrigger & operator = (PreScaleTrigger &&) = delete;
  void endJob() override; 


  // Required functions.
  bool filter(art::Event & e) override;
  void printParams();

private:

  int   _iPreScale;                                         ///; Pre-Scale for the trigger (pass every _iPreScale events)
  int _iEventCounter;                                       ///; Counter for events
  bool _bUseRndmPreScale;                                   ///; Switch to turn on a PreScale that uses a random number generator (pass ~ every _iPreScale events)
  std::default_random_engine _generator;                    ///; Random number generator
  std::uniform_int_distribution<> _rndmDistribution;        ///; Random number distribution
  int _iNumPassed;                                          ///; Number of events passing trigger
  int _iNumFailed;                                          ///; Number of events failing trigger

};


trig::PreScaleTrigger::PreScaleTrigger(fhicl::ParameterSet const & p)
// Initialize member data here.
{

  _iPreScale = p.get < int > ("PreScale", 2 );
  if(_iPreScale<1){
      std::cerr << "Error setting _iPreScale: " << _iPreScale << " Must be greater that 0" << std::endl;
      _iPreScale=1;
  }

  _iEventCounter = 0;
  _bUseRndmPreScale = p.get < bool > ("UseRndmPreScale", false);


  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  _generator = std::default_random_engine(seed);
  _rndmDistribution = std::uniform_int_distribution<> (0,_iPreScale-1);
  _iNumPassed=0;
  _iNumFailed=0;

  printParams();

}

void trig::PreScaleTrigger::printParams(){
  
  std::cout << std::string(80, '=') << std::endl;

  std::cout << "_iPreScale: " << _iPreScale << std::endl;
  std::cout << "_bUseRndmPreScale: " << _bUseRndmPreScale << std::endl;

  std::cout << std::string(80, '=') << std::endl;


}

bool trig::PreScaleTrigger::filter(art::Event & e)
{
  // Implementation of required member function here.
  auto eventID = e.id();

  _iEventCounter++;

  std::cout << "eventID " << eventID << " _iEventCounter " << _iEventCounter;
  
  if(_bUseRndmPreScale){
    if(_rndmDistribution(_generator)<1){
        std::cout << "  passed" << std::endl; 
        _iNumPassed++;
        return 1;
    }
    else{
        std::cout << "  failed" << std::endl; 
        _iNumFailed++;
        return 0;
    }

  }//_bUseRndmPreScale==true
  else{  
    if(_iEventCounter % _iPreScale == 0){
      std::cout << " passed" << std::endl;
        _iNumPassed++;
      return 1;
    } 
    else{
      std::cout << " failed" << std::endl;
      _iNumFailed++;
      return 0;
    } 
  } //_bUseRndmPreScale==false
}

void trig::PreScaleTrigger::endJob()
{

  std::cout << std::string(80, '+') << std::endl;
  std::cout << std::setw(6) << "Number Passed: " << std::setw(6) << _iNumPassed << "  fraction: " << double(_iNumPassed)/double(_iNumPassed+_iNumFailed)*100 << "%" << std::endl;
  std::cout << std::setw(6) << "Number Failed: " << std::setw(6) << _iNumFailed << "  fraction: " << double(_iNumFailed)/double(_iNumPassed+_iNumFailed)*100 << "%" << std::endl;
  std::cout << std::string(80, '+') << std::endl;
}

DEFINE_ART_MODULE(trig::PreScaleTrigger)