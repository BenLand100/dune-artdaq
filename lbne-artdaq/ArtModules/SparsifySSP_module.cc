////////////////////////////////////////////////////////////////////////
// Class:       SparsifySSP
// Module Type: producer
// File:        SparsifySSP_module.cc
// Description: Creates modified SSP fragments with triggers removed
//              when there is no interesting data in millislice.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Optional/TFileService.h"

#include "art/Utilities/Exception.h"
#include "lbne-raw-data/Overlays/SSPFragment.hh"
#include "lbne-raw-data/Overlays/SSPFragmentWriter.hh"
#include "lbne-raw-data/Overlays/PennMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/TpcMilliSliceFragment.hh"
#include "lbne-raw-data/Overlays/anlTypes.hh"
#include "lbne-raw-data/Overlays/FragmentType.hh"
#include "artdaq-core/Data/Fragments.hh"

#include "TFile.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <vector>
#include <iostream>
#include <limits>

namespace lbne {
  class SparsifySSP;
}

class lbne::SparsifySSP : public art::EDProducer {
public:
  explicit SparsifySSP(fhicl::ParameterSet const & pset);
  virtual ~SparsifySSP();

  void produce(art::Event & evt);

private:
  void beginJob() override;
  void endJob  () override;
  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > GetTriggers(art::Event const & evt);
  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > 
    SplitTriggersByFragment(art::Event const & evt,std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> >& evtTriggers);
  void RemoveRadiologicals(std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > & evtTriggers);
  bool EventHasPennTrigger(art::Event const & evt);
  bool EventHasRCEData(art::Event const & evt);
  void WriteMillislices(art::Event & evt,
			std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment,bool writeWaveforms);

  std::string raw_data_label_;
  std::string ssp_frag_type_;
  std::string penn_frag_type_;
  std::string rce_frag_type_;
  bool keep_waveforms_on_penntrig;
  bool keep_waveforms_on_rcedata;
  bool keep_all_waveforms;
  bool remove_radiologicals;
  unsigned long radio_search_window;
  unsigned int radio_ssp_thresh;

};

lbne::SparsifySSP::SparsifySSP(fhicl::ParameterSet const & pset)
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

void lbne::SparsifySSP::beginJob()
{
}

void lbne::SparsifySSP::endJob()
{
}

lbne::SparsifySSP::~SparsifySSP()
{
}

void lbne::SparsifySSP::produce(art::Event & evt)
{

  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > evtTriggers=this->GetTriggers(evt);
  if(remove_radiologicals){
    this->RemoveRadiologicals(evtTriggers);
  }
  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment;
  triggersByFragment=this->SplitTriggersByFragment(evt,evtTriggers);
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
  lbne::SparsifySSP::SplitTriggersByFragment(art::Event const & evt,std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> >& evtTriggers)
{

  std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment; 
  //Note triggersByModule value type is a reference. This gets set to point at the vectors in
  //triggersByFragment so that we can look for triggers by module id and put them into the right
  //fragment.
  std::map<unsigned int,std::vector<const SSPDAQ::EventHeader*>* > triggersByModule; 

  ///> look for raw SSP data  
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, ssp_frag_type_, raw);

  if (raw.isValid()){
    for (size_t idx = 0; idx < raw->size(); ++idx) {
      triggersByFragment[&((*raw)[idx])].clear();

      triggersByModule[(*raw)[idx].fragmentID()+1]=&triggersByFragment[&((*raw)[idx])];
    }
  }
  
  for(auto iTime=evtTriggers.begin();iTime!=evtTriggers.end();++iTime){
    for(auto iTrig=iTime->second.begin();iTrig!=iTime->second.end();++iTrig){
      unsigned int moduleId = ((*iTrig)->group2 & 0xFFF0) >> 4;
      triggersByModule.at(moduleId)->push_back(*iTrig);
    }
  }

  return triggersByFragment;
}

  
      


//Get pointers to the headers of all triggers in the event and map according to time
std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > lbne::SparsifySSP::GetTriggers(art::Event const & evt){

  std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > triggersByTime;

  ///> look for raw SSP data  
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, ssp_frag_type_, raw);

  if (raw.isValid()) {
    
    for (size_t idx = 0; idx < raw->size(); ++idx) {
      const auto& frag((*raw)[idx]);
      
      ///> Create an SSPFragment from the generic artdaq fragment
      SSPFragment sspf(frag);
      
      const SSPDAQ::MillisliceHeader* meta=0;
      if(frag.hasMetadata())
	{
	  meta = &(frag.metadata<SSPFragment::Metadata>()->sliceHeader); 
	}
      
      ///> get a pointer to the first trigger in the millislice
      const unsigned int* dataPointer = sspf.dataBegin();
      
      ///> loop over the triggers in the millislice
      unsigned int triggersProcessed=0;
      while((meta==0||triggersProcessed<meta->nTriggers)&&dataPointer<sspf.dataEnd()){

	///> get the trigger header
	const SSPDAQ::EventHeader* daqHeader=reinterpret_cast<const SSPDAQ::EventHeader*>(dataPointer);	
	unsigned long trig_timestamp_nova = ((unsigned long)daqHeader->timestamp[3] << 48) + ((unsigned long)daqHeader->timestamp[2] << 32)
	  + ((unsigned long)daqHeader->timestamp[1] << 16) + ((unsigned long)daqHeader->timestamp[0] << 0);
	
	triggersByTime[trig_timestamp_nova].push_back(daqHeader);

	///> increment the data pointer to the start of the next trigger
	dataPointer+=daqHeader->length;
      }//triggers
    }//fragments
  }//raw.IsValid()?
  return triggersByTime;
}


void lbne::SparsifySSP::RemoveRadiologicals(std::map<unsigned long,std::vector<const SSPDAQ::EventHeader*> > & evtTriggers){

  std::map<unsigned int,std::vector<unsigned long> > triggersByModule;
  unsigned long lastTriggerTime=0;

  for(auto iTime=evtTriggers.begin();iTime!=evtTriggers.end();++iTime){
    unsigned long triggerTime=iTime->first;
    if(lastTriggerTime&&triggerTime-lastTriggerTime>radio_search_window){

      if(triggersByModule.size()<radio_ssp_thresh){
	for(auto iModRemove=triggersByModule.begin();iModRemove!=triggersByModule.end();++iModRemove){
	  for(auto iTrigRemove=iModRemove->second.begin();iTrigRemove!=iModRemove->second.end();++iTrigRemove){
	    evtTriggers[*iTrigRemove].clear();
	  }
	}
      }
      triggersByModule.clear();
    }

    for(auto iTrigProcess=iTime->second.begin();iTrigProcess!=iTime->second.end();++iTrigProcess){
      unsigned int moduleId = ((*iTrigProcess)->group2 & 0xFFF0) >> 4;
      triggersByModule[moduleId].push_back(triggerTime);
    }
    lastTriggerTime=triggerTime;
  }
  if(triggersByModule.size()<radio_ssp_thresh){
    for(auto iModRemove=triggersByModule.begin();iModRemove!=triggersByModule.end();++iModRemove){
      for(auto iTrigRemove=iModRemove->second.begin();iTrigRemove!=iModRemove->second.end();++iTrigRemove){
	evtTriggers[*iTrigRemove].clear();
      }
    }
  }
}

bool lbne::SparsifySSP::EventHasRCEData(art::Event const & evt){
  ///> look for raw TpcMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, rce_frag_type_, raw);
  
  if (raw.isValid()) {
    
    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);
	
	///> Create a TpcMilliSliceFragment from the generic artdaq fragment
	TpcMilliSliceFragment msf(frag);
	
	///> get the number of MicroSlices in the MilliSlice
	lbne::TpcMilliSlice::Header::microslice_count_t msf_us_count = msf.microSliceCount();

	///> loop over the number of microslices
	for (uint32_t i_us = 0; i_us < msf_us_count; ++i_us)
	  {
	    ///> get the 'i_us-th' microslice
	    std::unique_ptr<const TpcMicroSlice> microslice = msf.microSlice(i_us);
	    
	    if (!microslice) {
	      throw cet::exception("Error in TpcMilliSliceDump module: unable to find requested microslice");
	    }
	    
	    ///> get the microslice sequence ID
	    //	    lbne::TpcMicroSlice::Header::sequence_id_t us_sequence_id = microslice->sequence_id();
	    
	    ///> get the microslice total size (header+data, in bytes)
	    //lbne::TpcMicroSlice::Header::microslice_size_t us_size = microslice->size();
	    
	    ///> get the number of NanoSlices in this MicroSlice
	    lbne::TpcMicroSlice::Header::nanoslice_count_t us_nanocount = microslice->nanoSliceCount();

	    if(us_nanocount){
	      std::cout<<"Slice has RCE data!"<<std::endl;
	      return true;
	    }
	  }
      } 
  }

  std::cout<<"No RCE data"<<std::endl;
  return false;
}

bool lbne::SparsifySSP::EventHasPennTrigger(art::Event const & evt){
  ///> look for raw PennMilliSlice data
  art::Handle<artdaq::Fragments> raw;
  evt.getByLabel(raw_data_label_, penn_frag_type_, raw);
  
  if (raw.isValid()) {
    
    for (size_t idx = 0; idx < raw->size(); ++idx)
      {
	const auto& frag((*raw)[idx]);
	
	///> Create a PennMilliSliceFragment from the generic artdaq fragment
	PennMilliSliceFragment msf(frag);
	
	///> Find the number of each type of payload word found in the millislice
	lbne::PennMilliSlice::Header::payload_count_t n_words_counter, n_words_trigger, n_words_timestamp;
	msf.payloadCount(n_words_counter, n_words_trigger, n_words_timestamp);
	
	//Only interested in if there are any triggers. Guessing this is how to do it?
	if(n_words_trigger>0) return true;
	
      }//raw fragment loop
  }//raw.IsValid()?
  
  return false;
}  


void lbne::SparsifySSP::WriteMillislices(art::Event & evt,
     std::map<artdaq::Fragment const *,std::vector<const SSPDAQ::EventHeader*> > triggersByFragment,bool writeWaveforms){

  std::unique_ptr<artdaq::Fragments> frags(new artdaq::Fragments);

  for(auto iFrag=triggersByFragment.begin();iFrag!=triggersByFragment.end();++iFrag){
    
    unsigned int dataSizeInWords=0;

    if(writeWaveforms){
      for(auto iTrig=iFrag->second.begin();iTrig!=iFrag->second.end();++iTrig){
	dataSizeInWords+=(*iTrig)->length;
      }
    }
    else{
      dataSizeInWords=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)*iFrag->second.size();
    }

    SSPDAQ::MillisliceHeader sliceHeader;
    sliceHeader.length=dataSizeInWords+SSPDAQ::MillisliceHeader::sizeInUInts;
    sliceHeader.nTriggers=iFrag->second.size();
          
    if(iFrag->first->hasMetadata())
      {
	const SSPDAQ::MillisliceHeader* origMeta = &(iFrag->first->metadata<SSPFragment::Metadata>()->sliceHeader);
	sliceHeader.startTime=origMeta->startTime;
	sliceHeader.endTime=origMeta->endTime;
      }
    
    SSPFragment::Metadata metadata;
    metadata.sliceHeader=sliceHeader;
    
    frags->emplace_back(*artdaq::Fragment::FragmentBytes(0,
							iFrag->first->sequenceID(), iFrag->first->fragmentID(),
							 lbne::detail::PHOTON, metadata) );
    
    SSPFragmentWriter newfrag(frags->back());
    newfrag.set_hdr_run_number(999);
    newfrag.resize(dataSizeInWords);
    
    unsigned int* dataPtr=&*newfrag.dataBegin();

    for(auto iTrig=iFrag->second.begin();iTrig!=iFrag->second.end();++iTrig){
      unsigned int const* trigPtr=(unsigned int const*)((void*)(*iTrig));
      if(writeWaveforms){
	std::copy(trigPtr,trigPtr+(*iTrig)->length,dataPtr);
	dataPtr+=(*iTrig)->length;
      }
      else{
	std::copy(trigPtr,trigPtr+sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int),dataPtr);
	SSPDAQ::EventHeader* fragEvHeader=(SSPDAQ::EventHeader*)((void*)(dataPtr));
	fragEvHeader->length=sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
	dataPtr+=fragEvHeader->length;
      }
    }
  }
  
  evt.put(std::move(frags),"PHOTON");
}


DEFINE_ART_MODULE(lbne::SparsifySSP)
