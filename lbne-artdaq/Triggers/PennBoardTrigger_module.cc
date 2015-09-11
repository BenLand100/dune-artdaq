////////////////////////////////////////////////////////////////////////
// Class:       PennBoardTrigger
// Module Type: filter
// File:        PennBoardTrigger_module.cc
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
#include <iomanip>
#include <bitset>

#include "artdaq-core/Data/Fragments.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/Utilities.hh"

namespace trig {
  class PennBoardTrigger;

  struct PTBTriggerPatterns{
    static uint8_t const bsu_rm_cl_bit = 26;
    static uint8_t const tsu_nu_sl_bit = 25;
    static uint8_t const tsu_sl_nu_bit = 24;
    static uint8_t const tsu_el_wu_bit = 23;
  };

  struct PTBTriggerTypes{
    static uint8_t const calibration=0x00;
    static uint8_t const ssp=0x08;
    static uint8_t const muon=0x10;
  };

  struct PTBTrigger{
    uint32_t trigger_pattern: 27;
    uint32_t trigger_type: 5;
  };
  struct PTBCounter{
    //I think that it goes 
    uint32_t tsu_wu: 10;
    uint32_t tsu_el: 10;
    uint32_t tsu_el_extra: 4;
    uint32_t tsu_nu: 6;
    uint32_t tsu_sl: 6;
    uint32_t tsu_nl: 6;
    uint32_t tsu_su: 6;
    uint32_t bsu_rm: 16;
    uint32_t bsu_cu: 10;
    uint32_t bsu_cl: 13;
    uint32_t bsu_rl: 10;
    
    /*
      pennboard_config.fcl from nuno
      BSU	: 0xFFFF				# BSU mask (RM 1-16)
      BSU	: 0x7F7E000000				# BSU mask (CL 1-13)
      TSU	: 0x3F000000			# TSU mask (NU 1-6)
      TSU	: 0xFC0000000			# TSU mask (SL 1-6)
      TSU	: 0xFC0000000000		# TSU mask (SU 1-6)
      TSU	: 0x3F000000000			# TSU mask (NL 1-6)
      TSU	: 0x7FE00				# TSU mask (EL 1-10)
      TSU	: 0x1FF					# TSU mask (WU 1-10)

      MonitoringData.cxx from Mike Wallbnk and Dominick Brailsford
    if (i>=0 && i<=9){
      //Dealing with the WU TSU counters                                                                                else if (i>=10 && i<=19){
      //Dealing with the EL TSU counters  
    else if (i>=20 && i<=23){
      //Dealing with the EL TSU counters                                                                                else if (i>=24 && i<=29){
      //Dealing with the NU TSU counters                                                                                else if (i>=30 && i<=35){
      //Dealing with the SL TSU counters                                                                                else if (i>=36 && i<=41){
      //Dealing with the NL TSU counters                                                                                else if (i>=42 && i<=47){
      //Dealing with the SU TSU counters                                                                                else if (i>=0+48 && i<=15+48){
      //Dealing with the RM BSU counters                                                                                else if (i>=16+48 && i<=25+48){
      //Dealing with the CU BSU counters                                                                                else if (i>=26+48 && i<=38+48){
      //Dealing with the CL BSU counters                                                                                else if (i>=39+48 && i<=48+48){
      //Dealing with the RL BSU counters                                                                            

    */
    /*
#TODO
#JPD - this looks like the REAL channel mapping - sent to Dominick Brailsford by nuno. Will implement next

## NOTE: 
========

The PTB has 50 input BSU channels. This was traced back in emails back to the beginning of the discussions. However, at the moment there are only 49 physical channels. Channel 32 in the PTB is not being used (would be ch 31 in CCU-3).

This means that the channels 33-49 are shifted in the output word becoming 32-48.


PTB Counter word bits to components
-----------------------------------

* Word has a total of 128 bits and contains:
   ++ 3 bits  : header mask
   ++ 28 bits : timestamp rollover (28 lab of timestamp)
   ++ 49 bits : BSU
   ++ 48 bits : TSU
 
word[127-125] : header[2-0]
word[124-97]  : ts_rollover[27-0]
word[96-48]   : BSU[48-0]
word[47-0]    : TSU[47-0]


BSU counter word to CCU counters:
---------------------------------

BSU[48-39] : RL 10-1 (CCU3 : 48-39)
BSU[38-32] : CL 13-7 (CCU3 : 38-32)
BSU[31-26] : CL 6-1  (CCU4 : 16-11)
BSU[25-16] : CU 10-1 (CCU4 : 10-1)
BSU[15-0]  : RM 16-1 (CCU4 : 40-25)


TSU counter word to TSU counters:
---------------------------------

TSU[47-42] : SU 6-1  (CCU2 : 24-19+48-43)
TSU[41-36] : NL 6-1  (CCU2 : 18-13+42-37)
TSU[35-30] : SL 6-1  (CCU2 : 12-7+36-31)
TSU[29-24] : NU 6-1  (CCU2 : 6-1+30-25)

TSU[23-20] : Extra   (CCU1 : 24-21+48-45)
TSU[19-10] : EL 10-1 (CCU1 : 20-11+44-35)
TSU[9-0]   : WU 10-1 (CCU1 : 10-1+34-25)
    */

  };
  
}

class trig::PennBoardTrigger : public art::EDFilter {
public:
  explicit PennBoardTrigger(fhicl::ParameterSet const & p);
  // The destructor generated by the compiler is fine for classes
  // without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  PennBoardTrigger(PennBoardTrigger const &) = delete;
  PennBoardTrigger(PennBoardTrigger &&) = delete;
  PennBoardTrigger & operator = (PennBoardTrigger const &) = delete;
  PennBoardTrigger & operator = (PennBoardTrigger &&) = delete;
  void beginJob();
  void printPennFragInfo( art::Handle<artdaq::Fragments> const & rawPTB);
  


  // Required functions.
  bool filter(art::Event & evt) override;
  void printParams();

private:

  bool fIsPTB; 
  std::string fPTBModuleLabel;
  std::string fPTBInstanceName;

};


void trig::PennBoardTrigger::beginJob(){
  printParams();
}

trig::PennBoardTrigger::PennBoardTrigger(fhicl::ParameterSet const & p)
// Initialize member data here.
{

  fPTBModuleLabel = p.get<std::string>("PTBModuleLabel", "daq");
  fPTBInstanceName = p.get<std::string>("PTBInstanceName", "TRIGGER");
}

void trig::PennBoardTrigger::printParams(){
  
  for(int i=0;i<80;i++) std::cerr << "=";
  std::cerr << std::endl;
  std::cerr << "fPTBModuleLabel: " << fPTBModuleLabel << std::endl;
  std::cerr << "fPTBInstanceName: " << fPTBInstanceName << std::endl;
  for(int i=0;i<80;i++) std::cerr << "=";
  std::cerr << std::endl;


}

bool trig::PennBoardTrigger::filter(art::Event & evt)
{
  // Implementation of required member function here.
  auto eventID = evt.id();

  art::Handle<artdaq::Fragments> rawPTB;
  fIsPTB=true;
  evt.getByLabel("daq","TRIGGER",rawPTB);
  //  evt.getByLabel(fPTBModuleLabel,fPTBInstanceName,rawPTB);

  if(rawPTB.isValid()){
    std::cerr << "INFO : Got Valid PTB fragments" << std::endl;
    std::cerr << "INFO : Got PTB art::Handle<artdaq::Fragments>" << std::endl;
    std::cerr << "INFO : eventID: " << eventID << std::endl;
    std::cerr << std::endl;
    //    printPennFragInfo(rawPTB);

    /*
      Loop over the fragments found.
      Make a PennMilliSliceFragment from each
      Check the number of payloads of each type within the fragement (referred to as 'frames')
      Get each payload in turn and check its type
      The Trigger and Counter types are of interest, so in that case we'll do something
      We should extract the bit patterns. Not sure on the best way to do this!
     */


    for (size_t frag_index=0; frag_index < rawPTB->size(); ++frag_index){
      const auto & frag((*rawPTB)[frag_index]); //Make a fragment from the frag_index element of rawPTB
      lbne::PennMilliSliceFragment ms_frag(frag);
      lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
      n_frames = ms_frag.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);
      
      //Information within the penn_payloads
      lbne::PennMicroSlice::Payload_Header::data_packet_type_t payload_type;
      lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
      uint8_t* payload_data;
      size_t payload_size;
      
      PTBTrigger *myPTBTrigger;
      PTBCounter *myPTBCounter;
      std::bitset<5> trigger_type;
      std::bitset<27> trigger_pattern;

      std::bitset<16> counter_bitset_tsu_wu       ;
      std::bitset<16> counter_bitset_tsu_el       ;
      std::bitset<16> counter_bitset_tsu_el_extra ;
      std::bitset<16> counter_bitset_tsu_nu       ;
      std::bitset<16> counter_bitset_tsu_sl       ;
      std::bitset<16> counter_bitset_tsu_nl       ;
      std::bitset<16> counter_bitset_tsu_su       ;
      std::bitset<16> counter_bitset_bsu_rm       ;
      std::bitset<16> counter_bitset_bsu_cu       ;
      std::bitset<16> counter_bitset_bsu_cl       ;
      std::bitset<16> counter_bitset_bsu_rl       ;



      //iterate through the frames
      for (uint32_t payload_index = 0; payload_index < n_frames; payload_index++){
	payload_data = ms_frag.payload(payload_index, payload_type, timestamp, payload_size);
	
	if (payload_data == nullptr ){
	  std::cerr << "payload_data == nullptr" << std::endl;
	}
	
	switch ( payload_type )
	  {
	  case lbne::PennMicroSlice::DataTypeCounter:
	    std::cerr << "payload_type: Counter   "   << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;

	    myPTBCounter = lbne::reinterpret_cast_checked<PTBCounter*>(payload_data);

	    std::cerr << "myPTBCounter size: " << sizeof(*myPTBCounter) << std::endl;

	    counter_bitset_tsu_wu       = std::bitset<16>(myPTBCounter->tsu_wu      ); 
	    counter_bitset_tsu_el       = std::bitset<16>(myPTBCounter->tsu_el      ); 
	    counter_bitset_tsu_el_extra = std::bitset<16>(myPTBCounter->tsu_el_extra); 
	    counter_bitset_tsu_nu       = std::bitset<16>(myPTBCounter->tsu_nu      ); 
	    counter_bitset_tsu_sl       = std::bitset<16>(myPTBCounter->tsu_sl      ); 
	    counter_bitset_tsu_nl       = std::bitset<16>(myPTBCounter->tsu_nl      ); 
	    counter_bitset_tsu_su       = std::bitset<16>(myPTBCounter->tsu_su      ); 
	    counter_bitset_bsu_rm       = std::bitset<16>(myPTBCounter->bsu_rm      ); 
	    counter_bitset_bsu_cu       = std::bitset<16>(myPTBCounter->bsu_cu      ); 
	    counter_bitset_bsu_cl       = std::bitset<16>(myPTBCounter->bsu_cl      ); 
	    counter_bitset_bsu_rl       = std::bitset<16>(myPTBCounter->bsu_rl      ); 

	    std::cerr << "tsu_wu      : " << counter_bitset_tsu_wu      << std::endl;
	    std::cerr << "tsu_el      : " << counter_bitset_tsu_el      << std::endl;
	    std::cerr << "tsu_el_extra: " << counter_bitset_tsu_el_extra<< std::endl;
	    std::cerr << "tsu_nu      : " << counter_bitset_tsu_nu      << std::endl;
	    std::cerr << "tsu_sl      : " << counter_bitset_tsu_sl      << std::endl;
	    std::cerr << "tsu_nl      : " << counter_bitset_tsu_nl      << std::endl;
	    std::cerr << "tsu_su      : " << counter_bitset_tsu_su      << std::endl;
	    std::cerr << "bsu_rm      : " << counter_bitset_bsu_rm      << std::endl;
	    std::cerr << "bsu_cu      : " << counter_bitset_bsu_cu      << std::endl;
	    std::cerr << "bsu_cl      : " << counter_bitset_bsu_cl      << std::endl;
	    std::cerr << "bsu_rl      : " << counter_bitset_bsu_rl      << std::endl;
	    std::cerr << std::endl;

	    break;
	  case lbne::PennMicroSlice::DataTypeTrigger:
	    std::cerr << "payload_type: Trigger   "   << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;

	    myPTBTrigger = lbne::reinterpret_cast_checked<PTBTrigger*>(payload_data);
	    trigger_type = std::bitset<5>(myPTBTrigger->trigger_type);
	    trigger_pattern = std::bitset<27> (myPTBTrigger->trigger_pattern);
	    std::cerr << "trigger_type:    " << trigger_type << std::endl;
	    std::cerr << "trigger_pattern: " << trigger_pattern << std::endl;


	    switch ( myPTBTrigger->trigger_type ){
	    case trig::PTBTriggerTypes::calibration:
	      std::cerr << "trig::PTBTriggerTypes::calibration" << std::endl;
	      break;
	    case trig::PTBTriggerTypes::ssp:
	      std::cerr << "trig::PTBTriggerTypes::ssp" << std::endl;
	      break;
	    case trig::PTBTriggerTypes::muon:
	      std::cerr << "trig::PTBTriggerTypes::muon" << std::endl;

	      if(trigger_pattern.test(trig::PTBTriggerPatterns::bsu_rm_cl_bit)) 
		std::cerr << "trig::PTBTriggerPatterns::bsu_rm_cl_bit" << std::endl;
	      if(trigger_pattern.test(trig::PTBTriggerPatterns::tsu_nu_sl_bit)) 
		std::cerr << "trig::PTBTriggerPatterns::tsu_nu_sl_bit" << std::endl;
	      if(trigger_pattern.test(trig::PTBTriggerPatterns::tsu_sl_nu_bit)) 
		std::cerr << "trig::PTBTriggerPatterns::tsu_sl_nu_bit" << std::endl;
	      if(trigger_pattern.test(trig::PTBTriggerPatterns::tsu_el_wu_bit)) 
		std::cerr << "trig::PTBTriggerPatterns::tsu_el_wu_bit" << std::endl;
	      break;
	    default:
	      std::cerr << "trig::PTBTriggerTypes::unkown" << std::endl;
	      break;
	    }//switch( myPTBTrigger->trigger_type)






	    break;
	  case lbne::PennMicroSlice::DataTypeTimestamp:
	    std::cerr << "payload_type: Timestamp " << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;
	    break;
	  case lbne::PennMicroSlice::DataTypeSelftest:
	    std::cerr << "payload_type: Selftest  "  << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;
	    break;
	  case lbne::PennMicroSlice::DataTypeChecksum:
	    std::cerr << "payload_type: Checksum  "  << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;
	    break;
	  default:
	    std::cerr << "payload_type: Unknown   "  << std::endl;
	    std::cerr << "payload_size: " << payload_size << std::endl;
	    break;
	  }//switch(payload_type)
	std::cerr << std::endl;
      }//payload_index
    }//frag_index
  }//isValid
  else{
    std::cerr << "INFO : NOT Got Valid PTB fragments" << std::endl;
  }

  return 1;
}



void trig::PennBoardTrigger::printPennFragInfo( art::Handle<artdaq::Fragments> const & rawPTB){

  std::cerr << "Found " << rawPTB->size() << " fragment(s) of type " << fPTBInstanceName << std::endl;

  for (size_t frag_index=0; frag_index < rawPTB->size(); ++frag_index){
    const auto & frag((*rawPTB)[frag_index]); //Make a fragment from the frag_index element of rawPTB

    lbne::PennMilliSliceFragment ms_frag(frag);
    //Get the number of each payload type in the millislice

    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = ms_frag.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);
    

    std::cerr << "Fragment " << frag.fragmentID() 
              << " with version " << ms_frag.version()
              << " and sequence ID " << ms_frag.sequenceID()
              << " consists of: " << ms_frag.size() << " bytes containing "
      //              << ms_frag.microSliceCount() << " microslices and" 
              << std::endl;
    std::cerr << n_frames << " total words ("
              << n_frames_counter << " counter + "
              << n_frames_trigger << " trigger + "
              << n_frames_timestamp << " timestamp + "
              << n_frames - (n_frames_counter + n_frames_trigger + n_frames_timestamp) << " selftest & checksum)"
              << std::endl;

    std::cerr << " with width " << ms_frag.widthTicks() 
              << " ticks (excluding overlap of " << ms_frag.overlapTicks()
              << " ticks_ and end timestamp " << ms_frag.endTimestamp() << std::endl;

    //Now we need to grab the payload informaation in the millislice
    lbne::PennMicroSlice::Payload_Header::data_packet_type_t payload_type;
    lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
    uint8_t* payload_data;
    size_t payload_size;

    //iterate through the frames
    for (uint32_t payload_index = 0; payload_index < n_frames; payload_index++){
      //Get the actual payload
      payload_data = ms_frag.payload(payload_index, payload_type, timestamp, payload_size);
      std::cerr << "payload_index: " << std::setw( 3) << std::dec << payload_index
                << " payload_size: " << std::setw( 3) << std::dec << payload_size
                << " timestamp: "    << std::setw(12) << std::dec << timestamp;

      // if ((payload_data == nullptr) || (payload_size == 0 )){
      //   std::cerr << std::endl;
      //   continue;
      // }
      if (payload_data == nullptr ){
	std::cerr << " payload_data == nullptr" << std::endl;
      }
      
      switch ( payload_type )
        {
        case lbne::PennMicroSlice::DataTypeCounter:
          std::cerr << " payload_type: Counter   "   ;//<< std::endl;
          break;
        case lbne::PennMicroSlice::DataTypeTrigger:
          std::cerr << " payload_type: Trigger   "   ;//<< std::endl;
          break;
        case lbne::PennMicroSlice::DataTypeTimestamp:
          std::cerr << " payload_type: Timestamp " ;//<< std::endl;
          break;
        case lbne::PennMicroSlice::DataTypeSelftest:
          std::cerr << " payload_type: Selftest  "  ;//<< std::endl;
          break;
        case lbne::PennMicroSlice::DataTypeChecksum:
          std::cerr << " payload_type: Checksum  "  ;//<< std::endl;
          break;
        default:
          std::cerr << " payload_type: Unknown   "   ;//<< std::endl;
          break;
        }//switch(payload_type)
      //Print out the bit masks for the trigger or counter packets
      if ( payload_type == lbne::PennMicroSlice::DataTypeCounter || payload_type == lbne::PennMicroSlice::DataTypeTrigger ){
	for( size_t payload_byte=0; payload_byte < payload_size; payload_byte++){
	  std::cerr << " " << std::bitset<8>( *(payload_data+payload_byte) );
	}
      }//if Counter or trigger payload
      std::cerr << std::endl;
    }//payload_index
  }//frag_index
}//printPennFragInfo

DEFINE_ART_MODULE(trig::PennBoardTrigger)
