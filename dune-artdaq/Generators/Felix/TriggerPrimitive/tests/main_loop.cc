//################################################################################
//# /*
//#        TimingMain.cpp
//#
//#  Giles Barr May 2017 for ProtoDUNE
//#
//# Standalone main program allowing to test the FragmentGenerator with two threads
//# Usage:
//#  ./TimingMain  <t>
//# The Parameter t is the time in milliseconds that the run should proceed before
//# the master thread stops the run.  Default value is 10 seconds.
//#
//# The sequence of steps taken here are
//#  (1) In main program, construct a TimingFragment_generator tfg:- this 
//#      does all of the configuration step
//#  (2) In main program, call tfg.start() - this gets the hardware 
//#      ready to start the run
//#  (3) In main program, start the data taking thread (function data_take 
//#      is the 'main' of the second thread)
//#  (4) data_take now calls tfg.getNext_() in a loop and prints one line 
//#      summary of any fragmenst it receives (combines summary every 100 calls 
//#      yielding nothing), and keeps doing this until tfg.getNext_() returns 
//#      false to signal end of data taking at which point the data_take can 
//#      terminate.  The first time this thread notices that the stopping_flag 
//#      variable in the base class is set, we call stop() [By doing it in 
//#      this thread, we have guaranteed we don't really need a mutex.
//#  (5) In the main program, after launching data_take, we wait for a time 
//#      (say 10 secs [Set from a run parameter]), call stop_nomutex() and 
//#      then set the stopping_flag variable in the commandable fragment 
//#      generator's base class. [In this order, so stop_nomutex() and 
//#      stop() are not called at the same time.
//#  (6) In the main program, we now wait for data_take to finish [thread 
//#      t.join() does this], and then we end the program.
//# */
//################################################################################

#include <thread>
#include <chrono>

#define protected public
#include "../../../FelixReceiver.hh"
#undef protected

#include "messagefacility/MessageLogger/MessageLogger.h"
#include "fhiclcpp/make_ParameterSet.h"

// Fiddle (static global: There should be a nicer way of allowing the 
// data_take() function to see the trg object)
dune::FelixReceiver* frag_gen_p = 0;

// --------------------------------------------

void data_take(int) {     // Main program of data taking thread
  printf("data_take: Started thread for GetNext\n");
  artdaq::FragmentPtrs frags;

  int seen_stop = 0;
  int nemptycalls = 0;
  bool ret = true;
  do {
    ret = frag_gen_p->getNext(frags);   // This returns true except when data taking is finished
    if (frags.size() == 0) {
      nemptycalls++;
      if (nemptycalls >= 3) {
        printf("Completed %d calls to getNext() with no data\n",nemptycalls);
        nemptycalls = 0;
      }
    } else {   // Frags has stome stuff in it
      //int i = 0;
      //for (auto frag : frags) {
        printf("Received data in %d frags\n",(int)frags.size());
      //} // End of frag loop
      frags.clear();
    } // endif

    if (seen_stop == 0 && frag_gen_p->should_stop() != 0) {  // First time we see the stopping flag
      seen_stop = 1;
      frag_gen_p->StopCmd(1000, 0);
      printf("data_take: called trg.stop()\n");
    }
  } while (ret);

  printf("getNext() returned false to indicate end of data taking, closing thread\n");
  return;
}

int main(int argn, char** argv) {

  int partition=0;
  int millisecwait = 30000;
  int enable_spill_commands_int = 1;
  if (argn > 4) {
    printf("usage: %s [partition] [millisec time (default 30s)] [enable spill commands]\n",argv[0]);
    return 1;
  }
  if(argn>=2)  sscanf(argv[1],"%d",&partition);
  if(argn>=3)  sscanf(argv[2],"%d",&millisecwait);
  if(argn>=4)  sscanf(argv[3],"%d",&enable_spill_commands_int);

  mf::setStandAloneMessageThreshold({"DEBUG"});

  printf("main: Configuring...\n");
  fhicl::ParameterSet ps;
  char* fclstring=new char[10000];
  std::ifstream fin("felix01_hw_cfg.fcl");
  fin.read(fclstring, 10000);
  fhicl::make_ParameterSet(fclstring, ps);
  dune::FelixReceiver frag_gen(ps);       // (1) This does the configure step
  frag_gen_p = &frag_gen;                        // Set this global variable to complete the fiddle at the start
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  printf("\nmain: Use start() to get to running state\n");
  frag_gen.StartCmd(1, 1000, 0);                     // (2) This takes us to the running state

  printf("\nmain: Now running, start data_taking thread\n");
  std::thread t1 {data_take,1};       // (3) This creates the data taking thread, which 
                                      //     starts immediately (page 1212 of BStroustrup
  printf("\nmain: After thread start - now waiting %d milliseconds for this run\n",millisecwait);

                                      // (4) Stuff done in data taking thread

  std::this_thread::sleep_for(std::chrono::milliseconds(millisecwait));
                                      // (5) Wait a time and assert the base class stop flag
  printf("\nmain: Wait over, stopping now\n");
  // trg.stopNoMutex();
  // trg.stop();
  frag_gen.StopCmd(1000, 0);

  t1.join();                          // (6) wait for data taking thread to close
  printf("\nmain: Thread has completed, we are finished\n");
  return 0;
}

