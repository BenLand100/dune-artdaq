////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.cxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#ifndef DataReformatter_cxx
#define DataReformatter_cxx

#include "DataReformatter.h"

bool _verbose = false;
int fRun = 0, fSubrun = 0, fEventNumber = 0;

void OnlineMonitoring::ReformatRCEBoardData(art::Handle<artdaq::Fragments>& rawRCE, DQMvector* RCEADCs) {

  unsigned int fNRCEChannels = 512;

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawRCE->size() << " fragment(s) of type RCE" << std::endl;

  // fNumSubDetectorsPresent += rawRCE->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  std::map<unsigned int, unsigned int> tpcFragmentMap;
  for (unsigned int fragIt = 0; fragIt < rawRCE->size(); ++fragIt) {
    const artdaq::Fragment &fragment = ((*rawRCE)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID(); //+ 100;
    tpcFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
  }

  // Loop over channels
  for (unsigned int chanIt = 0; chanIt < fNRCEChannels; ++chanIt) {

    // Vector of ADC values for this channel
    std::vector<int> adcVector;

    // Find the fragment ID and the sample for this channel
    unsigned int fragmentID = (unsigned int)((chanIt/128)+100);
    unsigned int sample = (unsigned int)(chanIt%128);

    // Analyse this fragment if it exists
    if (tpcFragmentMap.find(fragmentID) != tpcFragmentMap.end() ) {

      if (_verbose)
      	std::cout << "%%DQM---------- Channel " << chanIt << ", fragment " << fragmentID << " and sample " << sample << std::endl;

      // Find the millislice fragment this channel lives in
      unsigned int fragIndex = tpcFragmentMap.at(fragmentID);

      // Get the millislice fragment
      const artdaq::Fragment &frag = ((*rawRCE)[fragIndex]);
      lbne::TpcMilliSliceFragment millisliceFragment(frag);

      // Number of microslices in millislice fragments
      auto nMicroSlices = millisliceFragment.microSliceCount();

      // hNumMicroslicesInMillislice->SetBinContent(fragmentID,nMicroSlices);

      if (_verbose)
      	std::cout << "%%DQM--------------- TpcMilliSlice fragment " << fragmentID << " contains " << nMicroSlices << " microslices" << std::endl;

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	// Get the microslice
	std::unique_ptr <const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
	auto nNanoSlices = microslice->nanoSliceCount();

	if (_verbose)
	  std::cout << "%%DQM-------------------- TpcMicroSlice " << microIt << " contains " << nNanoSlices << " nanoslices" << std::endl;

	for (unsigned int nanoIt = 0; nanoIt < nNanoSlices; ++nanoIt) {

	  // receivedData = true;

	  // Get the ADC value
	  uint16_t adc = std::numeric_limits<uint16_t>::max();
	  bool success = microslice->nanosliceSampleValue(nanoIt, sample, adc);

	  if (success)
	    adcVector.push_back((int)adc);

	} // nanoslice loop

      } // microslice loop

    } // analyse fragment loop

    (*RCEADCs).push_back(adcVector);

  } // channel loop

  // if (!receivedData && fEventNumber%1000==0)
  //   mf::LogWarning("Monitoring") << "No nanoslices have been made after " << fEventNumber << " events";

}


void OnlineMonitoring::ReformatSSPBoardData(art::Handle<artdaq::Fragments>& rawSSP, DQMvector* SSPADCs) {

  unsigned int fNSSPChannels = 96;

  if (_verbose)
    std::cout << "%%DQM----- Run " << fRun << ", subrun " << fSubrun << ", event " << fEventNumber << " has " << rawSSP->size() << " fragment(s) of type PHOTON" << std::endl;

  // fNumSubDetectorsPresent += rawSSP->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  std::map<unsigned int, unsigned int> sspFragmentMap;
  for (unsigned int fragIt = 0; fragIt < rawSSP->size(); ++fragIt) {
    const artdaq::Fragment &fragment = ((*rawSSP)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID();
    sspFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
  }

  // Keep a note of the previous fragment
  unsigned int prevFrag = 100;

  // Define a data pointer
  const unsigned int *dataPointer = 0;

  // Loop over optical channels
  for (unsigned int opChanIt = 0; opChanIt < fNSSPChannels; ++opChanIt) {

    std::vector<int> waveformVector;

    // Find the fragment ID and the sample for this channel
    unsigned int fragmentID = (unsigned int)(opChanIt/12);
    unsigned int sample = (unsigned int)(opChanIt%12);

    // Check this fragment exists
    if (sspFragmentMap.find(fragmentID) != sspFragmentMap.end() ) {

      if (_verbose)
      	std::cout << "%%DQM---------- Optical Channel " << opChanIt << ", fragment " << fragmentID << " and sample " << sample << std::endl;

      // Find the fragment this channel lives in
      unsigned int fragIndex = sspFragmentMap[fragmentID];

      // Get the millislice fragment
      const artdaq::Fragment &frag = ((*rawSSP)[fragIndex]);
      lbne::SSPFragment sspFragment(frag);

      // Define at start of each fragment
      if (fragmentID != prevFrag) dataPointer = sspFragment.dataBegin();
      prevFrag = fragmentID;

      // Check there is data in this fragment
      if (dataPointer >= sspFragment.dataEnd())
	continue;

      // Get the event header
      const SSPDAQ::EventHeader *daqHeader = reinterpret_cast<const SSPDAQ::EventHeader*> (dataPointer);

      unsigned short OpChan = ((daqHeader->group2 & 0x000F) >> 0);
      unsigned int nADC = (daqHeader->length-sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int))*2;

      if (_verbose)
      	std::cout << "%%DQM--------------- OpChan " << OpChan << ", number of ADCs " << nADC << std::endl;

      // Move data to end of trigger header (start of ADCs) and copy it
      dataPointer += sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
      const unsigned short *adcPointer = reinterpret_cast<const unsigned short*> (dataPointer);

      for (unsigned int adcIt = 0; adcIt < nADC; ++adcIt) {
	const unsigned short *adc = adcPointer + adcIt;
	waveformVector.push_back(*adc);
      }

      // Move to end of trigger
      dataPointer += nADC/2;

    } // analyse fragment loop

    (*SSPADCs).push_back(waveformVector);

  } // channel loop

}

OnlineMonitoring::PTBFormatter::PTBFormatter(art::Handle<artdaq::Fragments> rawPTB){
  //Initialise the trigger rates
  fMuonTriggerRates[1] = 0;
  fMuonTriggerRates[2] = 0;
  fMuonTriggerRates[4] = 0;
  fMuonTriggerRates[8] = 0;

  //Loop over the generic fragments
  for (size_t idx = 0; idx < rawPTB->size(); ++idx){
    const auto& frag((*rawPTB)[idx]);

    //Get the PennMilliSliceFragment from the artdaq fragment
    lbne::PennMilliSliceFragment msf(frag);

    //Get the number of each payload type in the millislice
    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = msf.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);

    //Now we need to grab the payload information in the millislice
    lbne::PennMicroSlice::Payload_Header::data_packet_type_t type;
    lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
    uint8_t* payload_data;
    size_t payload_size;

    //Loop over the payloads
    for (uint32_t ip = 0; ip < n_frames; ip++){
      //Get the actual payload
      payload_data = msf.payload(ip, type, timestamp, payload_size);

      unsigned int payload_type = (unsigned int) type;
      //Loop over the words in the payload
      if (!((payload_data != nullptr) && payload_size)) continue;
      switch (payload_type){
        case 1:
          //Dealing with counter word
          CollectCounterBits(payload_data, payload_size);
          fCounterTimes.push_back(timestamp);
          break;
        case 2:
          //Dealing with trigger word
          CollectMuonTrigger(payload_data, payload_size);
          fMuonTriggerTimes.push_back(timestamp);
        default:
          break;
      }
    }
  }
}

void OnlineMonitoring::PTBFormatter::AnalyzeCounter(int counter_index, double &activation_time, int &hit_rate) const{
  //We need to loop through the requested counter to check
  //  A) when it switched back on
  //  B) How many times it switched on
  //For now, we are going to ingore the counter if it begins the millislice activated UNTIL it switches off

  //Initialise the values to be returned
  activation_time = 0.;
  hit_rate = 0;
  //Assume that the counter was on in the previous millislice (this shouldn't be detrimental to the logic)
  bool counter_previously_on = true;

  for (std::vector<std::bitset<OnlineMonitoring::TypeSizes::CounterWordSize> >::const_iterator countIt = fCounterBits.begin(); countIt != fCounterBits.end(); countIt++){
    //Get the counter status
    bool counter_currently_on = (*countIt)[counter_index];
    //std::cout<<counter_currently_on<<" "<<fCounterTimes[std::distance(countIt,fCounterBits.end())]<<std::endl;
    if (counter_previously_on && counter_currently_on){
      //If the counter was previously on and it is still on, just continue
      continue;
    }
    else if (counter_previously_on && !counter_currently_on){
      //The counter WAS on but it is now off so record the counter as switching off and move on
      counter_previously_on = false;
      continue;
    }
    else if (!counter_previously_on && !counter_currently_on){
      //The counter was off and it is still off so very little to do here.  Continue!
      continue;
    }
    else if (!counter_previously_on && counter_currently_on){
      //The counter has switched on!!!!!
      //Record that it is now on and record some numbers
      counter_previously_on = true;
      hit_rate++;
      //Also record the activation time if it is the first time the counter has been hit
      if (hit_rate==1){
        //We need the array index to fetch the timestamp of the payload
        int index = std::distance(countIt, fCounterBits.end());
        activation_time = fCounterTimes.at(index);
      }
    }
    else{
      //We should never get here
      std::cout<<"ERROR IN PTBFORMATTER'S LOGIC IN COUNTER ANALYSIS"<<std::endl;
    }
  }

  return;
}

void OnlineMonitoring::PTBFormatter::AnalyzeMuonTrigger(int trigger_number, int &trigger_rate) {

  trigger_rate = fMuonTriggerRates[trigger_number];
  return;
}


void OnlineMonitoring::PTBFormatter::CollectCounterBits(uint8_t* payload, size_t payload_size){
  std::bitset<OnlineMonitoring::TypeSizes::CounterWordSize> bits;
  for (size_t ib = 0; ib < payload_size; ib++){
    std::bitset<OnlineMonitoring::TypeSizes::CounterWordSize> byte = payload[ib];
    //bits ^= (byte << 8*(payload_size-(ib+1)));
    bits ^= (byte << 8*ib);

  }

  fCounterBits.push_back(bits);
  return;
}

void OnlineMonitoring::PTBFormatter::CollectMuonTrigger(uint8_t* payload, size_t payload_size){
  std::bitset<OnlineMonitoring::TypeSizes::TriggerWordSize> bits;
  for (size_t ib = 0; ib < payload_size; ib++){
    std::bitset<OnlineMonitoring::TypeSizes::TriggerWordSize> byte = payload[ib];
    bits ^= (byte << 8*ib);
    //bits ^= (byte << 8*(payload_size-(ib)));
    //std::cout<<std::bitset<8>(payload[ib])<<std::endl;
  }
  //Bits collected, now get the trigger type
  std::bitset<OnlineMonitoring::TypeSizes::TriggerWordSize> trigger_type_bits; 
  trigger_type_bits ^= (bits >> (OnlineMonitoring::TypeSizes::TriggerWordSize - 5));
  int trigger_type = static_cast<int>(trigger_type_bits.to_ulong());
  //If we have a muon trigger, get which trigger it was and increase the hit rate
  if (trigger_type == 16){
    std::bitset<OnlineMonitoring::TypeSizes::TriggerWordSize> muon_trigger_bits;
    //Shift the bits left by 5 first to remove the trigger type bits
    muon_trigger_bits ^= (bits << 5);
    //Now we need to shift the entire thing set to the LSB so we can record the number
    //The muon trigger pattern words are 4 bits
    muon_trigger_bits >>= (OnlineMonitoring::TypeSizes::TriggerWordSize-4);
    int muon_trigger = static_cast<int>(muon_trigger_bits.to_ulong());
    fMuonTriggerRates[muon_trigger]++;

  }
  return;
}

void OnlineMonitoring::ReformatTriggerBoardData(art::Handle<artdaq::Fragments> rawPTB) {
  std::cout<<"PTB size: "<<rawPTB->size()<<std::endl;

  //Loop over the generic fragments
  for (size_t idx = 0; idx < rawPTB->size(); ++idx){
    const auto& frag((*rawPTB)[idx]);

    //Get the PennMilliSliceFragment from the artdaq fragment
    lbne::PennMilliSliceFragment msf(frag);

    //Get the number of each payload type in the millislice
    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = msf.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);

    //Now we need to grab the payload information in the millislice
    lbne::PennMicroSlice::Payload_Header::data_packet_type_t type;
    lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp;
    uint8_t* payload_data;
    size_t payload_size;

    //Loop over the payloads
    for (uint32_t ip = 0; ip < n_frames; ip++){
      //Get the actual payload
      payload_data = msf.payload(ip, type, timestamp, payload_size);

      unsigned int payload_type = (unsigned int) type;
      std::cout<<"TYPE: " << payload_type << std::endl;
      //Loop over the words in the payload
      if (!((payload_data != nullptr) && payload_size)) continue;
      for (size_t ib = 0; ib < payload_size; ib++){
        switch (payload_type){
          case 1:
            //Dealing with counter word
            break;
          case 2:
            //Dealing with trigger word
            std::cout<<"Found trigger word"<<std::endl;
          default:
            break;
        }


      }
    }


  }
}


void OnlineMonitoring::WindowingRCEData(DQMvector& ADCs, std::vector<int>* numBlocks, std::vector<std::vector<short> >* blocksBegin, std::vector<std::vector<short> >* blocksSize) {


  const int fWindowingZeroThresholdSigned = 10;
  int fWindowingNearestNeighbour = 4;

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {

    // Define variables for the windowing
    std::vector<short> tmpBlockBegin((ADCs.at(channel).size()/2)+1);
    std::vector<short> tmpBlockSize((ADCs.at(channel).size()/2)+1);
    int tmpNumBlocks = 0;
    int blockstartcheck = 0;
    int endofblockcheck = 0;

    for (int tick = 0; tick < (int)ADCs.at(channel).size(); ++tick) {
      // Windowing code (taken from raw.cxx::ZeroSuppression)
      if (blockstartcheck == 0) {
	if (ADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  if (tmpNumBlocks > 0) {
	    if (tick-fWindowingNearestNeighbour <= tmpBlockBegin[tmpNumBlocks-1] + tmpBlockSize[tmpNumBlocks-1]+1) {
	      tmpNumBlocks--;
	      tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	      blockstartcheck = 1;
	    }
	    else {
	      tmpBlockBegin[tmpNumBlocks] = (tick - fWindowingNearestNeighbour > 0) ? tick - fWindowingNearestNeighbour : 0;
	      tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	      blockstartcheck = 1;
	    }
	  }
	  else {
	    tmpBlockBegin[tmpNumBlocks] = (tick - fWindowingNearestNeighbour > 0) ? tick - fWindowingNearestNeighbour : 0;
	    tmpBlockSize[tmpNumBlocks] = tick - tmpBlockBegin[tmpNumBlocks] + 1;
	    blockstartcheck = 1;	    
	  }
	}
      }
      else if (blockstartcheck == 1) {
	if (ADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  ++tmpBlockSize[tmpNumBlocks];
	  endofblockcheck = 0;
	}
	else {
	  if (endofblockcheck < fWindowingNearestNeighbour) {
	    ++endofblockcheck;
	    ++tmpBlockSize[tmpNumBlocks];
	  }
	  //block has ended
	  else if ( std::abs(ADCs.at(channel).at(tick+1)) <= fWindowingZeroThresholdSigned || std::abs(ADCs.at(channel).at(tick+2)) <= fWindowingZeroThresholdSigned) {  
	    endofblockcheck = 0;
	    blockstartcheck = 0;
	    ++tmpNumBlocks;
	  }
	}
      }

    } // tick

    (*blocksBegin).push_back(tmpBlockBegin);
    (*blocksSize).push_back(tmpBlockSize);
    (*numBlocks).push_back(tmpNumBlocks);

  } // channel

}


#endif
