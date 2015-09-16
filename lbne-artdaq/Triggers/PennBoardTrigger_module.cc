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
#include <sstream>

#include "artdaq-core/Data/Fragments.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/Utilities.hh"

namespace trig {
  class PennBoardTrigger;
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
  void checkGetNextPayload(art::Handle<artdaq::Fragments> const & rawPTB);
  void printPayloadInfo(art::Handle<artdaq::Fragments> const & rawPTB);
  bool filterOnTriggerPayload(art::Handle<artdaq::Fragments> const & rawPTB);

  // Required functions.
  bool filter(art::Event & evt) override;
  void printParams();

private:

  bool fIsPTB; 
  std::string fPTBModuleLabel;
  std::string fPTBInstanceName;
  bool fVerbose;

  bool fFilterOnTriggerType;
  lbne::PennMilliSlice::TriggerPayload::trigger_type_t fTriggerType;
  bool fFilterOnTriggerPattern;
  lbne::PennMilliSlice::TriggerPayload::trigger_bits_t fTriggerPatternBit;

};


void trig::PennBoardTrigger::beginJob(){
  printParams();
}

trig::PennBoardTrigger::PennBoardTrigger(fhicl::ParameterSet const & p)
// Initialize member data here.
{
  fPTBModuleLabel = p.get<std::string>("PTBModuleLabel", "daq");
  fPTBInstanceName = p.get<std::string>("PTBInstanceName", "TRIGGER");
  fVerbose = p.get<bool>("Verbose", false);

  fFilterOnTriggerType = p.get<bool>("FilterOnTriggerType", false);
  fTriggerType = p.get<lbne::PennMilliSlice::TriggerPayload::trigger_type_t>("TriggerType", 0x00);

  fFilterOnTriggerPattern = p.get<bool>("FilterOnTriggerPattern", false);
  fTriggerPatternBit = p.get<lbne::PennMilliSlice::TriggerPayload::trigger_bits_t>("TriggerPatternBit", 1);
}

void trig::PennBoardTrigger::printParams(){
  
  for(int i=0;i<80;i++) std::cerr << "=";
  std::cerr << std::endl;
  std::cerr << "fPTBModuleLabel: " << fPTBModuleLabel << std::endl;
  std::cerr << "fPTBInstanceName: " << fPTBInstanceName << std::endl;
  std::cerr << "fVerbose: " << fVerbose << std::endl;  

  std::cerr << "fFilterOnTriggerType: " << fFilterOnTriggerType << std::endl;
  std::cerr << "fTriggerType: 0x" << std::hex << static_cast<int>(fTriggerType) << std::dec << std::endl;
  std::cerr << "fFilterOnTriggerPattern: " << fFilterOnTriggerPattern << std::endl;
  std::cerr << "fTriggerPatternBit: " << fTriggerPatternBit << std::endl;
  for(int i=0;i<80;i++) std::cerr << "=";
  std::cerr << std::endl;

}

bool trig::PennBoardTrigger::filter(art::Event & evt)
{

  art::Handle<artdaq::Fragments> rawPTB;
  fIsPTB=true;
  evt.getByLabel(fPTBModuleLabel,fPTBInstanceName,rawPTB);

  if(fFilterOnTriggerType) return filterOnTriggerPayload(rawPTB);
  else printPayloadInfo(rawPTB);

  return false;
}

bool trig::PennBoardTrigger::filterOnTriggerPayload(art::Handle<artdaq::Fragments> const & rawPTB){

  if(rawPTB.isValid()){
    std::ostringstream my_ostringstream;
    for(int i=0;i<80;i++) my_ostringstream << "=";
    my_ostringstream << std::endl;
    my_ostringstream << "INFO : filterOnTriggerType" << std::endl;
    for(int i=0;i<80;i++) my_ostringstream << "=";
    my_ostringstream << std::endl;

    if(fVerbose) std::cerr << my_ostringstream.str();
    my_ostringstream.str("");
    my_ostringstream.clear();

    for (size_t frag_index=0; frag_index < rawPTB->size(); ++frag_index){
      const auto & frag((*rawPTB)[frag_index]); //Make a fragment from the frag_index element of rawPTB
      lbne::PennMilliSliceFragment ms_frag(frag);
      lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
      n_frames = ms_frag.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);
      
      lbne::PennMicroSlice::Payload_Header::data_packet_type_t payload_type;
      lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
      uint8_t* payload_data=0;
      size_t payload_size=0;    
      
      //iterate through the frames
      for(uint32_t payload_index = 0; payload_index < n_frames; payload_index++){
        payload_data = ms_frag.get_next_payload(payload_index, payload_type, timestamp, payload_size);
        //The following should not happen, but just in case the get_next_payload runs off the end of the payload buffer
        if(payload_data==nullptr) continue;
    
        if(payload_type == lbne::PennMicroSlice::DataTypeTrigger){
    
          lbne::PennMilliSlice::TriggerPayload *myTriggerPayload = lbne::reinterpret_cast_checked<lbne::PennMilliSlice::TriggerPayload*>(payload_data);      
          std::bitset<lbne::PennMilliSlice::TriggerPayload::num_bits_trigger_type> this_trigger_type;
          std::bitset<lbne::PennMilliSlice::TriggerPayload::num_bits_trigger_pattern> this_trigger_pattern;
    
          this_trigger_type = std::bitset<lbne::PennMilliSlice::TriggerPayload::num_bits_trigger_type>(myTriggerPayload->trigger_type);
          this_trigger_pattern = std::bitset<lbne::PennMilliSlice::TriggerPayload::num_bits_trigger_pattern> (myTriggerPayload->trigger_pattern);
          
          if((myTriggerPayload->trigger_type) == fTriggerType){
    
            if(fFilterOnTriggerPattern){
              if(this_trigger_pattern.test(fTriggerPatternBit)){
                my_ostringstream << "PASS - myTriggerPayload->trigger_pattern bit " << fTriggerPatternBit << " == 1" << std::endl;
                my_ostringstream << "PASS - payload_index:    " << payload_index << std::endl;
                my_ostringstream << "PASS - trigger_type:    " << this_trigger_type << std::endl;
                my_ostringstream << "PASS - timestamp:       " << timestamp << std::endl;
                my_ostringstream << "PASS - trigger_pattern: " << this_trigger_pattern << std::endl;
                my_ostringstream << std::endl;
                if(fVerbose) std::cerr << my_ostringstream.str();
                my_ostringstream.str("");
                my_ostringstream.clear();
    
                return true;
              }
              else{
                my_ostringstream << "    FAIL - myTriggerPayload->trigger_pattern bit " << fTriggerPatternBit << " != 1" << std::endl;
                my_ostringstream << "    FAIL - payload_index:    " << payload_index << std::endl;
                my_ostringstream << "    FAIL - trigger_type:    " << this_trigger_type << std::endl;
                my_ostringstream << "    FAIL - timestamp:       " << timestamp << std::endl;
                my_ostringstream << "    FAIL - trigger_pattern: " << this_trigger_pattern << std::endl;
                my_ostringstream << std::endl;
                if(fVerbose) std::cerr << my_ostringstream.str();
                my_ostringstream.str("");
                my_ostringstream.clear();
    
              }//this_trigger_pattern.test(fTriggerPatternBit) == false
            }//fFilterOnTriggerPattern == true
            else{
              my_ostringstream << "PASS - myTriggerPayload->trigger_type == trigger_type" << std::endl;
              my_ostringstream << "PASS - payload_index:    " << payload_index << std::endl;
              my_ostringstream << "PASS - trigger_type:    " << this_trigger_type << std::endl;
              my_ostringstream << "PASS - timestamp:       " << timestamp << std::endl;
              my_ostringstream << "PASS - trigger_pattern: " << this_trigger_pattern << std::endl;
              my_ostringstream << std::endl;
              if(fVerbose) std::cerr << my_ostringstream.str();
              my_ostringstream.str("");
              my_ostringstream.clear();
    
              return true;          
            }//fFilterOnTriggerPattern == false
          }
          else
            {
              my_ostringstream << "  FAIL - myTriggerPayload->trigger_type != trigger_type" << std::endl;
              my_ostringstream << "  FAIL - payload_index:    " << payload_index << std::endl;
              my_ostringstream << "  FAIL - trigger_type:    " << this_trigger_type << std::endl;
              my_ostringstream << "  FAIL - timestamp:       " << timestamp << std::endl;
              my_ostringstream << "  FAIL - trigger_pattern: " << this_trigger_pattern << std::endl;
              my_ostringstream << std::endl;
              if(fVerbose) std::cerr << my_ostringstream.str();
              my_ostringstream.str("");
              my_ostringstream.clear();
    
            }
        }//DataTypeTrigger
      }//payload_index
    }//frag_index
  }//rawPTB.isValid

  return false;
}//filterOnTriggerType

void trig::PennBoardTrigger::printPayloadInfo(art::Handle<artdaq::Fragments> const & rawPTB){

  if(rawPTB.isValid()){
    std::cerr << "INFO : Got Valid PTB fragments" << std::endl;
    std::cerr << "INFO : Got PTB art::Handle<artdaq::Fragments>" << std::endl;
    std::cerr << std::endl;
    // checkGetNextPayload(rawPTB);

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
      
      //      PTBTrigger *myPTBTrigger;
      //      PTBCounter *myPTBCounter;
      lbne::PennMilliSlice::CounterPayload *myCounterPayload;
      lbne::PennMilliSlice::TriggerPayload *myTriggerPayload;

      std::bitset<5> trigger_type;
      std::bitset<27> trigger_pattern;

      std::bitset<10> counter_bitset_tsu_wu       ;//tsu_wu     : 10
      std::bitset<10> counter_bitset_tsu_el       ;//tsu_el     : 10
      std::bitset< 4> counter_bitset_tsu_extra    ;//tsu_extra  :  4
      std::bitset< 6> counter_bitset_tsu_nu       ;//tsu_nu     :  6
      std::bitset< 6> counter_bitset_tsu_sl       ;//tsu_sl     :  6
      std::bitset< 6> counter_bitset_tsu_nl       ;//tsu_nl     :  6
      std::bitset< 6> counter_bitset_tsu_su       ;//tsu_su     :  6
      std::bitset<16> counter_bitset_bsu_rm       ;//bsu_rm     : 16
      std::bitset<10> counter_bitset_bsu_cu       ;//bsu_cu     : 10
      std::bitset<13> counter_bitset_bsu_cl       ;//bsu_cl     : 13
      std::bitset<10> counter_bitset_bsu_rl       ;//bsu_rl     : 10
                             
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

        //        myPTBCounter = lbne::reinterpret_cast_checked<PTBCounter*>(payload_data);
        myCounterPayload = lbne::reinterpret_cast_checked<lbne::PennMilliSlice::CounterPayload*>(payload_data);

        std::cerr << "myCounterPayload size: " << sizeof(*myCounterPayload) << std::endl;
        //        std::cerr << "myCounterPayload size: " << sizeof(PTBCounter) << std::endl;
        std::cerr << "myCounterPayload size: " << sizeof(lbne::PennMilliSlice::CounterPayload) << std::endl;

        counter_bitset_tsu_wu       = std::bitset<10>(myCounterPayload->tsu_wu      );// tsu_wu     : 10
        counter_bitset_tsu_el       = std::bitset<10>(myCounterPayload->tsu_el      );// tsu_el     : 10
        counter_bitset_tsu_extra    = std::bitset< 4>(myCounterPayload->tsu_extra   );// tsu_extra  :  4
        counter_bitset_tsu_nu       = std::bitset< 6>(myCounterPayload->tsu_nu      );// tsu_nu     :  6
        counter_bitset_tsu_sl       = std::bitset< 6>(myCounterPayload->tsu_sl      );// tsu_sl     :  6
        counter_bitset_tsu_nl       = std::bitset< 6>(myCounterPayload->tsu_nl      );// tsu_nl     :  6
        counter_bitset_tsu_su       = std::bitset< 6>(myCounterPayload->tsu_su      );// tsu_su     :  6
        counter_bitset_bsu_rm       = std::bitset<16>(myCounterPayload->bsu_rm      );// bsu_rm     : 16
        counter_bitset_bsu_cu       = std::bitset<10>(myCounterPayload->bsu_cu      );// bsu_cu     : 10
        counter_bitset_bsu_cl       = std::bitset<13>(myCounterPayload->bsu_cl      );// bsu_cl     : 13
        counter_bitset_bsu_rl       = std::bitset<10>(myCounterPayload->bsu_rl      );// bsu_rl     : 10

        std::cerr << "tsu_wu      : " << counter_bitset_tsu_wu      << std::endl;
        std::cerr << "tsu_el      : " << counter_bitset_tsu_el      << std::endl;
        std::cerr << "tsu_extra   : " << counter_bitset_tsu_extra   << std::endl;
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

        //        myPTBTrigger = lbne::reinterpret_cast_checked<PTBTrigger*>(payload_data);
        myTriggerPayload = lbne::reinterpret_cast_checked<lbne::PennMilliSlice::TriggerPayload*>(payload_data);

        trigger_type = std::bitset<5>(myTriggerPayload->trigger_type);
        trigger_pattern = std::bitset<27> (myTriggerPayload->trigger_pattern);
        std::cerr << "trigger_type:    " << trigger_type << std::endl;
        std::cerr << "trigger_pattern: " << trigger_pattern << std::endl;


        switch ( myTriggerPayload->trigger_type ){
        case lbne::PennMilliSlice::TriggerPayload::TriggerTypes::calibration:
          std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerTypes::calibration" << std::endl;
          break;
        case lbne::PennMilliSlice::TriggerPayload::TriggerTypes::ssp:
          std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerTypes::ssp" << std::endl;
          break;
        case lbne::PennMilliSlice::TriggerPayload::TriggerTypes::muon:
          std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerTypes::muon" << std::endl;

          if(trigger_pattern.test(lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::bsu_rm_cl)) 
        std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::bsu_rm_cl" << std::endl;
          if(trigger_pattern.test(lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_nu_sl)) 
        std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_nu_sl" << std::endl;
          if(trigger_pattern.test(lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_sl_nu)) 
        std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_sl_nu" << std::endl;
          if(trigger_pattern.test(lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_el_wu)) 
        std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerPatternBits::tsu_el_wu" << std::endl;
          break;
        default:
          std::cerr << "lbne::PennMilliSlice::TriggerPayload::TriggerTypes::unkown" << std::endl;
          break;
        }//switch( myTriggerPayload->trigger_type)

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
}

void trig::PennBoardTrigger::checkGetNextPayload(art::Handle<artdaq::Fragments> const & rawPTB){
  for (size_t frag_index=0; frag_index < rawPTB->size(); ++frag_index){
    const auto & frag((*rawPTB)[frag_index]); //Make a fragment from the frag_index element of rawPTB
    lbne::PennMilliSliceFragment ms_frag(frag);
    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = ms_frag.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);

    lbne::PennMicroSlice::Payload_Header::data_packet_type_t payload_type;
    lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
    uint8_t* payload_data;
    size_t payload_size;

    lbne::PennMicroSlice::Payload_Header::data_packet_type_t payload_type_get_next;
    lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp_get_next;
    uint8_t* payload_data_get_next;
    size_t payload_size_get_next;
    
    
    uint32_t payload_index_get_next=0;
    //iterate through the frames
    for (uint32_t payload_index = 0; payload_index < n_frames; payload_index++){
      payload_data = ms_frag.payload(payload_index, payload_type, timestamp, payload_size);
      payload_data_get_next = ms_frag.get_next_payload(payload_index_get_next, payload_type_get_next, timestamp_get_next, payload_size_get_next);
    
      if(payload_data_get_next == nullptr){
    std::cerr << "ERROR - payload_data_get_next == nullptr" << std::endl;
    return;
      }

      if(payload_index != payload_index_get_next) {
    std::cerr << "MISSMATCH - payload_index != payload_index_get_next" << std::endl;
      }
      else if(payload_data != payload_data_get_next){
    std::cerr << "MISSMATCH - payload_data != payload_data_get_next" << std::endl;
      }
      else if(payload_type != payload_type_get_next){
    std::cerr << "MISSMATCH - payload_type != payload_type_get_next" << std::endl;
      }
      else if(timestamp != timestamp_get_next){
    std::cerr << "MISSMATCH - timestamp != timestamp_get_next" << std::endl;
      }
      else if(payload_size != payload_size_get_next){
    std::cerr << "MISSMATCH - payload_size != payload_size_get_next" << std::endl;
      }
      else{
    std::cerr << "MATCH - payload_index: " << payload_index << std::endl;
      }
    }//payload_index
  }//frag_index
}



DEFINE_ART_MODULE(trig::PennBoardTrigger)
