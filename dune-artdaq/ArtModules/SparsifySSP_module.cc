////////////////////////////////////////////////////////////////////////
// Class:       SparsifySSP
// Module Type: producer
// File:        SparsifySSP_module.cc
// Description: Creates modified SSP fragments with uninteresting data removed.
//              -Removes "radiologicals" where only one SSP sees triggers within
//               a user-defined search window
//              -Truncates triggers to headers if RCE data or Penn triggers are
//               absent from millislice (exact behaviour defined in .fcl)
////////////////////////////////////////////////////////////////////////

//framework includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "canvas/Utilities/Exception.h"

//data type includes
#include "dune-raw-data/Overlays/SSPFragment.hh"
#include "dune-raw-data/Overlays/SSPFragmentWriter.hh"
#include "dune-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "dune-raw-data/Overlays/anlTypes.hh"
#include "dune-raw-data/Overlays/FragmentType.hh"
#include "artdaq-core/Data/Fragments.hh"

//C++ and STL includes
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <vector>
#include <iostream>

namespace dune {
  class SparsifySSP;
}

class dune::SparsifySSP : public art::EDProducer {
public:
  explicit SparsifySSP(fhicl::ParameterSet const & pset);
  virtual ~SparsifySSP();

  void produce(art::Event & evt);

private:
  //Currently empty
  void beginJob() override;

  //Currently empty
  void endJob  () override;

  //Return map keyed by timestamp; values are vectors of all SSP triggers occuring at a given time in the event.
  //Triggers from all fragments are returned in the same vector.
  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > GetTriggers(art::Event const & evt);

  //Convert map keyed by timestamp to map keyed by original PHOTON fragment, with all triggers associated
  //with that fragment stored in the value
  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > 
    SplitTriggersByFragment(art::Event const & evt,std::map<unsigned long,
			    std::vector<const SSPDAQ::EventHeader*> >& evtTriggers);

  //Modify input map to remove all triggers which don't appear close in time with triggers on other SSPs
  //(exact behaviour defined in .fcl)
  void RemoveRadiologicals(std::map<unsigned long,
			   std::vector<const SSPDAQ::EventHeader*> > & evtTriggers);

  //Return whether a Penn trigger word is present in any of the TRIGGER fragments in the event
  bool EventHasPennTrigger(art::Event const & evt);

  //Return whether any nanoslices are present in any of the TPC fragments in the event
  bool EventHasRCEData(art::Event const & evt);

  //Create output millislice fragments and put into the event. One fragment will be written for each
  //key in the input map, and filled with all triggers in the value vector.
  //If writeWaveforms is false, triggers will be truncated and only headers will be put into the fragments.
  void WriteMillislices(art::Event & evt,
			std::map<artdaq::Fragment const *,
			std::vector<const SSPDAQ::EventHeader*> > triggersByFragment,
			bool writeWaveforms);

  //Module which created the original fragments (expect this to be "daq")
  std::string raw_data_label_;

  //Types of the input fragments (expect these to be "PHOTON", "TRIGGER", "TPC")
  std::string ssp_frag_type_;
  std::string penn_frag_type_;
  std::string rce_frag_type_;

  //Store full waveforms if there is at least one penn trigger word in the millislice
  bool keep_waveforms_on_penntrig;

  //Store full waveforms if there is at least one RCE nanoslice in the millislice
  bool keep_waveforms_on_rcedata;

  //Always store full waveforms
  bool keep_all_waveforms;

  //Completely discard triggers if there are not triggers on other SSPs close in time
  bool remove_radiologicals;

  //Length of time between adjacent triggers at which we end the search window for radiologicals
  //(in ticks)
  unsigned long radio_search_window;

  //Number of SSPs which need to be triggered within a search window for triggers to be kept
  unsigned int radio_ssp_thresh;

};

//Constructor just loads configuration from .fcl and registers
//PHOTON fragments as product
dune::SparsifySSP::SparsifySSP(fhicl::ParameterSet const & pset)
    : raw_data_label_(pset.get<std::string>("raw_data_label")),
      ssp_frag_type_(pset.get<std::string>("ssp_frag_type")),
      penn_frag_type_(pset.get<std::string>("penn_frag_type")),
      rce_frag_type_(pset.get<std::string>("rce_frag_type")),
      keep_waveforms_on_penntrig(pset.get<bool>("keep_waveforms_on_penntrig")),
      keep_waveforms_on_rcedata(pset.get<bool>("keep_waveforms_on_rcedata")),
      keep_all_waveforms(pset.get<bool>("keep_all_waveforms")),
      remove_radiologicals(pset.get<bool>("remove_radiologicals")),
      radio_search_window(pset.get<unsigned long>("radio_search_window")),
      radio_ssp_thresh(pset.get<unsigned int>("radio_ssp_thresh"))
{
  produces<artdaq::Fragments>("PHOTON");
}

void dune::SparsifySSP::beginJob()
{
}

void dune::SparsifySSP::endJob()
{
}

dune::SparsifySSP::~SparsifySSP()
{
}

void dune::SparsifySSP::produce(art::Event & evt)
{

  //Find all SSP triggers in event
  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > evtTriggers=this->GetTriggers(evt);

  //Remove isolated triggers occuring on only a small number of SSPs
  if(remove_radiologicals){
    this->RemoveRadiologicals(evtTriggers);
  }

  //Re-order remaining triggers to store by fragment, ready to write to millislice
  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment;
  triggersByFragment=this->SplitTriggersByFragment(evt,evtTriggers);

  //Write either full triggers or headers to event depending on what data is on other
  //systems in this slice
  if(keep_all_waveforms||
     (this->EventHasPennTrigger(evt)&&keep_waveforms_on_penntrig)||
     (this->EventHasRCEData(evt)&&keep_waveforms_on_rcedata)){
    WriteMillislices(evt,triggersByFragment,true);
  }
  else{
    WriteMillislices(evt,triggersByFragment,false);
  }
}

std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > 
  dune::SparsifySSP::SplitTriggersByFragment(art::Event const & evt,std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> >& evtTriggers)
{

  //Declare output map
  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment; 

  //Note triggersByModule value type is a pointer. This gets set to point at the vectors in
  //triggersByFragment so that we can look for triggers by module id and put them into the correct
  //fragment.
  std::map<unsigned int,std::vector<const SSPDAQ::EventHeader*>* > triggersByModule; 

  /// Find all fragments in original SSP data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, ssp_frag_type_, raw);

  if (raw.isValid()){
    for (size_t idx = 0; idx < raw->size(); ++idx) {
      //Create output map entry for this fragment
      triggersByFragment[&((*raw)[idx])].clear();

      //Set vector* in by-module map to point at relevant vector in output map
      triggersByModule[(*raw)[idx].fragmentID()+1]=&triggersByFragment[&((*raw)[idx])];
    }
  }
  
  //Put all triggers in the event into the correct module/fragment
  for(auto iTime=evtTriggers.begin();iTime!=evtTriggers.end();++iTime){
    for(auto iTrig=iTime->second.begin();iTrig!=iTime->second.end();++iTrig){
      unsigned int moduleId = ((*iTrig)->group2 & 0xFFF0) >> 4;
      triggersByModule.at(moduleId)->push_back(*iTrig);
    }
  }

  return triggersByFragment;
}

  
      


//Get pointers to the headers of all triggers in the event and map according to time
std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > dune::SparsifySSP::GetTriggers(art::Event const & evt){

  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > triggersByTime;

  // look for raw SSP data  
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, ssp_frag_type_, raw);

  if (raw.isValid()) {
    
    //Iterate over fragments in event
    for (size_t idx = 0; idx < raw->size(); ++idx) {
      const auto& frag((*raw)[idx]);
      
      // Create an SSPFragment from the generic artdaq fragment
      SSPFragment sspf(frag);
      
      //Get metadata from fragment
      const SSPDAQ::MillisliceHeader* meta=0;
      if(frag.hasMetadata())
	{
	  meta = &(frag.metadata<SSPFragment::Metadata>()->sliceHeader); 
	}
      
      // get a pointer to the first trigger in the millislice
      const unsigned int* dataPointer = sspf.dataBegin();
      
      // loop over the triggers in the millislice
      unsigned int triggersProcessed=0;
      while((meta==0||triggersProcessed<meta->nTriggers)&&dataPointer<sspf.dataEnd()){

	// get the trigger header
	const SSPDAQ::EventHeader* daqHeader=reinterpret_cast<const SSPDAQ::EventHeader*>(dataPointer);	
	unsigned long trig_timestamp_nova = ((unsigned long)daqHeader->timestamp[3] << 48) + ((unsigned long)daqHeader->timestamp[2] << 32)
	  + ((unsigned long)daqHeader->timestamp[1] << 16) + ((unsigned long)daqHeader->timestamp[0] << 0);
	
	triggersByTime[trig_timestamp_nova].push_back(daqHeader);

	// increment the data pointer to the start of the next trigger
	dataPointer+=daqHeader->length;
      }//triggers
    }//fragments
  }//raw.IsValid()?
  return triggersByTime;
}


void dune::SparsifySSP::RemoveRadiologicals(std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > & evtTriggers){
  
  //Fill triggers into map by module so we can count how many modules have triggers
  //in a search window by checking size of map
  std::map<unsigned int,std::vector<unsigned long> > triggersByModule;
  unsigned long lastTriggerTime=0;

  //loop over all triggers in event
  for(auto iTime=evtTriggers.begin();iTime!=evtTriggers.end();++iTime){
    unsigned long triggerTime=iTime->first;

    //If gap between this trigger and previous trigger is bigger than search window
    //then check how many modules were present in previous set of triggers
    if(lastTriggerTime&&triggerTime-lastTriggerTime>radio_search_window){

      //If fewer modules are present in search window than threshold parameter
      //then remove all triggers in search window from list
      if(triggersByModule.size()<radio_ssp_thresh){
	for(auto iModRemove=triggersByModule.begin();iModRemove!=triggersByModule.end();++iModRemove){
	  for(auto iTrigRemove=iModRemove->second.begin();iTrigRemove!=iModRemove->second.end();++iTrigRemove){
	    evtTriggers[*iTrigRemove].clear();
	  }
	}
      }
      //Clear list of triggers so we can start a new search window
      triggersByModule.clear();
    }

    //Put present trigger into list
    for(auto iTrigProcess=iTime->second.begin();iTrigProcess!=iTime->second.end();++iTrigProcess){
      unsigned int moduleId = ((*iTrigProcess)->group2 & 0xFFF0) >> 4;
      triggersByModule[moduleId].push_back(triggerTime);
    }
    lastTriggerTime=triggerTime;
  }
  //After processing all triggers, check final search window and remove these triggers too
  //if not enough modules fired
  if(triggersByModule.size()<radio_ssp_thresh){
    for(auto iModRemove=triggersByModule.begin();iModRemove!=triggersByModule.end();++iModRemove){
      for(auto iTrigRemove=iModRemove->second.begin();iTrigRemove!=iModRemove->second.end();++iTrigRemove){
	evtTriggers[*iTrigRemove].clear();
      }
    }
  }
}

bool dune::SparsifySSP::EventHasRCEData(art::Event const & evt){

  // look for raw TpcMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, rce_frag_type_, raw);
  
  if (raw.isValid()) {
    
    //Iterate over all RCE fragments in event
    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);
	
	// Create a TpcMilliSliceFragment from the generic artdaq fragment
	TpcMilliSliceFragment msf(frag);
	
	// get the number of MicroSlices in the MilliSlice
	dune::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();

	///> loop over the microslices
	for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us)
	  {
	    // get the 'i_us-th' microslice
	    std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);
	    
	    if (!microslice) {
	      throw cet::exception("Error in TpcMilliSliceDump module: unable to find requested microslice");
	    }
	    	    
	    ///> get the number of NanoSlices in this MicroSlice
	    dune::TpcMicroSlice::Header::nanoslice_count_t us_nanocount = microslice->nanoSliceCount();

	    //If there are any nanoslices in the microslice then there is RCE data present in the slice
	    //so return true
	    if(us_nanocount){
	      return true;
	    }
	  }
      } 
  }

  //Checked all fragments and saw no data - return false
  return false;
}

bool dune::SparsifySSP::EventHasPennTrigger(art::Event const & evt){
  // look for raw PennMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, penn_frag_type_, raw);
  
  if (raw.isValid()) {
    
    //Iterate over all Penn fragments in event
    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);
	
	// Create a PennMilliSliceFragment from the generic artdaq fragment
	PennMilliSliceFragment msf(frag);
	
	// Find the number of each type of payload word found in the millislice
	dune::PennMilliSlice::Header::payload_count_t n_words_counter, n_words_trigger, n_words_timestamp;
	msf.payloadCount(n_words_counter, n_words_trigger, n_words_timestamp);
	
	//Only interested in if there are any triggers. Guessing this is how to do it?
	if(n_words_trigger>0) return true;
	
      }//raw fragment loop
  }//raw.IsValid()?
  
  //Checked all fragments and saw no triggers - return false
  return false;
}  


void dune::SparsifySSP::WriteMillislices(art::Event & evt,
     std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment,bool writeWaveforms){

  //Create new fragments object to put in event
  std::unique_ptr<artdaq::Fragments> frags(new artdaq::Fragments);

  //Iterate over all fragments that were present in original SSP data
  for(auto iFrag=triggersByFragment.begin();iFrag!=triggersByFragment.end();++iFrag){
    
    //Work out how much space is needed in fragment for data
    unsigned int dataSizeInWords=0;

    //If writing waveforms then sum size of triggers
    if(writeWaveforms){
      for(auto iTrig=iFrag->second.begin();iTrig!=iFrag->second.end();++iTrig){
	dataSizeInWords+=(*iTrig)->length;
      }
    }
    //If not writing waveforms then data size is total size of headers
    else{
      dataSizeInWords=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)*iFrag->second.size();
    }

    //Build a header for the new fragment
    SSPDAQ::MillisliceHeader sliceHeader;
    sliceHeader.length=dataSizeInWords+SSPDAQ::MillisliceHeader::sizeInUInts;
    sliceHeader.nTriggers=iFrag->second.size();
          
    //Get slice start and end time from metadata in original fragment
    if(iFrag->first->hasMetadata())
      {
	const SSPDAQ::MillisliceHeader* origMeta = &(iFrag->first->metadata<SSPFragment::Metadata>()->sliceHeader);
	sliceHeader.startTime=origMeta->startTime;
	sliceHeader.endTime=origMeta->endTime;
      }
    
    //Build metadata object from the header we just built, and use it for a new
    //fragment that we amplace at back of output Fragments vector
    SSPFragment::Metadata metadata;
    metadata.sliceHeader=sliceHeader;
    
    frags->emplace_back(*artdaq::Fragment::FragmentBytes(0,
							iFrag->first->sequenceID(), iFrag->first->fragmentID(),
							 dune::detail::PHOTON, metadata) );
    
    //Create SSPFragmentWriter overlay to enable us to write data to new fragment
    SSPFragmentWriter newfrag(frags->back());
    
    //Is this right??
    newfrag.set_hdr_run_number(999);

    //Resize fragment to hold the amount of data we calculated earlier
    newfrag.resize(dataSizeInWords);
    
    //Start writing at beginning of data in the new fragment
    unsigned int* dataPtr=&*newfrag.dataBegin();

    //Iterate over all triggers which need to be written to the fragment
    for(auto iTrig=iFrag->second.begin();iTrig!=iFrag->second.end();++iTrig){
      
      //The data we will copy from starts at the pointer to the trigger header
      //that is stored in the input map
      unsigned int const* trigPtr=(unsigned int const*)((void*)(*iTrig));

      //If writing waveforms we will go beyond the header and copy the waveform data
      //beyond it too
      if(writeWaveforms){
	std::copy(trigPtr,trigPtr+(*iTrig)->length,dataPtr);
	dataPtr+=(*iTrig)->length;
      }
      //If not, we will just write the header data to the fragment
      else{
	std::copy(trigPtr,trigPtr+sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int),dataPtr);
	SSPDAQ::EventHeader* fragEvHeader=(SSPDAQ::EventHeader*)((void*)(dataPtr));

	//Remember we must alter the trigger header in the new fragment
	//to say that the trigger length is only the header length!
	fragEvHeader->length=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
	dataPtr+=fragEvHeader->length;
      }
    }
  }
  
  //Write out all our new fragments to the event
  evt.put(std::move(frags),"PHOTON");
}


DEFINE_ART_MODULE(dune::SparsifySSP)
