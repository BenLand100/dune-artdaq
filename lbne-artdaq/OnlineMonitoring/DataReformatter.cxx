
////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.cxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#include "DataReformatter.hxx"

// RCE ----------------------------------------------------------------------------------------------------------------------------------------------------
OnlineMonitoring::RCEFormatter::RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE) {

  if (!rawRCE.isValid()) {
    NumRCEs = 0;
    return;
  }

  this->AnalyseADCs(rawRCE);
  this->Windowing();

}

void OnlineMonitoring::RCEFormatter::AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE) {

  NumRCEs = rawRCE->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  std::map<unsigned int, unsigned int> tpcFragmentMap;
  for (unsigned int fragIt = 0; fragIt < rawRCE->size(); ++fragIt) {
    const artdaq::Fragment &fragment = ((*rawRCE)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID(); //+ 100;
    tpcFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
    std::stringstream stream; stream << std::setfill('0') << std::setw(2) << fragmentID-100;
    RCEsWithData.push_back(std::string("RCE")+stream.str());
  }

  // Loop over channels
  for (unsigned int chanIt = 0; chanIt < NRCEChannels; ++chanIt) {

    // Vector of ADC values for this channel
    std::vector<int> adcVector;

    // Find the fragment ID and the sample for this channel
    unsigned int fragmentID = (unsigned int)((chanIt/128)+100);
    unsigned int sample = (unsigned int)(chanIt%128);

    // Analyse this fragment if it exists
    if (tpcFragmentMap.find(fragmentID) != tpcFragmentMap.end() ) {

      // Find the millislice fragment this channel lives in
      unsigned int fragIndex = tpcFragmentMap.at(fragmentID);

      // Get the millislice fragment
      const artdaq::Fragment &frag = ((*rawRCE)[fragIndex]);
      lbne::TpcMilliSliceFragment millisliceFragment(frag);

      // Number of microslices in millislice fragments
      auto nMicroSlices = millisliceFragment.microSliceCount();

      // hNumMicroslicesInMillislice->SetBinContent(fragmentID,nMicroSlices);

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	// Get the microslice
	std::unique_ptr <const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
	auto nNanoSlices = microslice->nanoSliceCount();

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

    ADCs.push_back(adcVector);

  } // channel loop

  // if (!receivedData && fEventNumber%1000==0)
  //   mf::LogWarning("Monitoring") << "No nanoslices have been made after " << fEventNumber << " events";

}

void OnlineMonitoring::RCEFormatter::Windowing() {

  const int fWindowingZeroThresholdSigned = 10;
  int fWindowingNearestNeighbour = 4;

  for (unsigned int channel = 0; channel < ADCs.size(); ++channel) {

    // Define variables for the windowing
    std::vector<short> tmpBlockBegin((ADCs.at(channel).size()/2)+1);
    std::vector<short> tmpBlockSize((ADCs.at(channel).size()/2)+1);
    int tmpNumBlocks = 0;
    int blockstartcheck = 0;
    int endofblockcheck = 0;

    for (unsigned int tick = 0; tick < ADCs.at(channel).size(); ++tick) {
      // Windowing code (taken from raw.cxx::ZeroSuppression)
      if (blockstartcheck == 0) {
	if (ADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  if (tmpNumBlocks > 0) {
	    if ((int)tick-fWindowingNearestNeighbour <= tmpBlockBegin[tmpNumBlocks-1] + tmpBlockSize[tmpNumBlocks-1]+1) {
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
	  else if ( (tick+1 < ADCs.at(channel).size() and std::abs(ADCs.at(channel).at(tick+1)) <= fWindowingZeroThresholdSigned) or
		    (tick+2 < ADCs.at(channel).size() and std::abs(ADCs.at(channel).at(tick+2)) <= fWindowingZeroThresholdSigned) ) {  
	    endofblockcheck = 0;
	    blockstartcheck = 0;
	    ++tmpNumBlocks;
	  }
	}
      }

    } // tick

    fWindowingBlockBegin.push_back(tmpBlockBegin);
    fWindowingBlockSize.push_back(tmpBlockSize);
    fWindowingNumBlocks.push_back(tmpNumBlocks);

  } // channel

}


// SSP ----------------------------------------------------------------------------------------------------------------------------------------------------
OnlineMonitoring::SSPFormatter::SSPFormatter(art::Handle<artdaq::Fragments> const& rawSSP) {

  /// SSPFormatter takes raw SSP artdaq::Fragments and extracts the information, saving useful bits

  if (!rawSSP.isValid()) {
    NumSSPs = 0;
    return;
  }
  NumSSPs = rawSSP->size();

  for (unsigned int sspchan = 0; sspchan < NSSPChannels; ++sspchan)
    fChannelTriggers[sspchan] = {};

  for (unsigned int fragmentNum = 0; fragmentNum < rawSSP->size(); ++fragmentNum) {

    // Get the raw fragment
    const artdaq::Fragment fragment = rawSSP->at(fragmentNum);
    lbne::SSPFragment sspfrag(fragment);

    // Note this SSP has data (and format name as DAQ would)
    std::stringstream stream; stream << std::setfill('0') << std::setw(2) << fragment.fragmentID()+1;
    SSPsWithData.push_back(std::string("SSP")+stream.str());

    // Get the metadata
    const SSPDAQ::MillisliceHeader* meta = 0;
    if (fragment.hasMetadata())
      meta = &(fragment.metadata<lbne::SSPFragment::Metadata>()->sliceHeader);
    
    const unsigned int* dataPointer = sspfrag.dataBegin();

    // Loop over all the triggers in this millislice
    for (unsigned int triggerNum = 0; (meta == 0 or triggerNum < meta->nTriggers) and dataPointer < sspfrag.dataEnd(); ++triggerNum) {

      const SSPDAQ::EventHeader* triggerHeader = reinterpret_cast<const SSPDAQ::EventHeader*>(dataPointer);

      //DBB: Get the NOvA timestamp
      unsigned long nova_timestamp = ((unsigned long)triggerHeader->timestamp[3] << 48) + ((unsigned long)triggerHeader->timestamp[2] << 32) + ((unsigned long)triggerHeader->timestamp[1] << 16) + ((unsigned long)triggerHeader->timestamp[0] << 0);

      // Find the (online) 'channel number' [this doesn't make much sense!]
      int SSPChannel = ((triggerHeader->group2 & 0x000F) >> 0);
      int SSPNumber  = ((triggerHeader->group2 & 0x00F0) >> 4);
      int channel = (12 * (SSPNumber-1)) + SSPChannel;

      // Find the information for this trigger
      unsigned int peaksum = ((triggerHeader->group3 & 0x00FF) >> 16) + triggerHeader->peakSumLow;
      if (peaksum & 0x00800000)
        peaksum |= 0xFF000000;
      unsigned int prerise = ((triggerHeader->group4 & 0x00FF) << 16) + triggerHeader->preriseLow;
      unsigned int integral = ((unsigned int)(triggerHeader->intSumHigh) << 8) + (((unsigned int)(triggerHeader->group4) & 0xFF00) >> 8);
      unsigned int pedestal = triggerHeader->baseline;
      unsigned int nTicks = (triggerHeader->length - sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int)) * 2;

      // Move data to look at ADCs
      dataPointer += sizeof(SSPDAQ::EventHeader)/sizeof(unsigned int);
      const unsigned short* adcPointer = reinterpret_cast<const unsigned short*>(dataPointer);

      // Make ADC vector
      std::vector<int> adcVector;
      for (size_t adcIt = 0; adcIt < nTicks; ++adcIt) {
	const unsigned short* adc = adcPointer + adcIt;
	adcVector.push_back(*adc);
      }

      // Find the mean and RMS ADC for this trigger
      double mean = TMath::Mean(adcVector.begin(),adcVector.end());
      double rms  = TMath::RMS (adcVector.begin(),adcVector.end());

      // Move data pointer to end of the trigger ready for next one
      dataPointer += nTicks / 2;

      // Save the information for this trigger in the reformatter object
      Trigger trigger(channel, peaksum, prerise, integral, pedestal, nTicks, mean, rms, adcVector, nova_timestamp);
      fTriggers.push_back(trigger);
      fChannelTriggers.at(channel).push_back(trigger);

    } // triggers

  } // millislices

}


// PTB ----------------------------------------------------------------------------------------------------------------------------------------------------
OnlineMonitoring::PTBFormatter::PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB) {

  if (!rawPTB.isValid()) {
      PTBData = false;
      return;
  }
  PTBData = true;

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
      if (type==2){
        //lbne::PennMicroSlice::Payload_Header* payload_header = reinterpret_cast<lbne::PennMicroSlice::Payload_Header*>(pl_ptr);
      }
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

void OnlineMonitoring::PTBFormatter::AnalyzeCounter(int counter_index, double& activation_time, int& hit_rate) const {
  //We need to loop through the requested counter to check
  //  A) when it switched back on
  //  B) How many times it switched on
  //For now, we are going to ingore the counter if it begins the millislice activated UNTIL it switches off

  //Initialise the values to be returned
  activation_time = 0.;
  hit_rate = 0;
  //Assume that the counter was on in the previous millislice (this shouldn't be detrimental to the logic)
  bool counter_previously_on = true;

  for (std::vector<std::bitset<TypeSizes::CounterWordSize> >::const_iterator countIt = fCounterBits.begin(); countIt != fCounterBits.end(); countIt++){
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

void OnlineMonitoring::PTBFormatter::AnalyzeMuonTrigger(int trigger_number, int& trigger_rate) const {
  trigger_rate = fMuonTriggerRates.at(trigger_number);
  return;
}

void OnlineMonitoring::PTBFormatter::CollectCounterBits(uint8_t* payload, size_t payload_size) {
  std::bitset<TypeSizes::CounterWordSize> bits;

//  for (size_t ib = 0; ib < payload_size; ib++){
  for (size_t ib = 0; ib < payload_size; ib++){
    std::bitset<TypeSizes::CounterWordSize> byte = payload[ib];
    //std::cout<<std::bitset<8>(payload[ib])<<std::endl;
    //std::cout<<byte<<std::endl;
    //bits ^= (byte << 8*(payload_size-(ib+1)));
    bits ^= (byte << 8*ib);
    //std::cout<<std::bitset<8>(payload[ib])<<std::endl;
  }
  std::cout<<bits<<std::endl;
  //std::cout<<std::endl;
  //std::cout<<bits<<std::endl;
  //ReverseBits(bits);
  //std::cout<<bits<<std::endl<<std::endl;

  fCounterBits.push_back(bits);
  return;
}

void OnlineMonitoring::PTBFormatter::CollectMuonTrigger(uint8_t* payload, size_t payload_size) {
  std::bitset<TypeSizes::TriggerWordSize> bits;
  for (size_t ib = 0; ib < payload_size; ib++){
    //std::bitset<8> new_bits = payload[ib];
    //ReverseBits(new_bits);
    std::bitset<TypeSizes::TriggerWordSize> byte = payload[ib];
    //std::bitset<TypeSizes::TriggerWordSize> byte;
    //for (int i = 0; i < 8; i++) byte[i] = new_bits[i];
    //byte=new_bits<<8;
    bits ^= (byte << 8*ib);
    //bits ^= (byte << 8*(payload_size-(ib+1)));
    //std::cout<<std::bitset<8>(payload[ib])<<std::endl;
  }
  //std::cout<<std::endl;
  //ReverseBits(bits);
  //std::cout<<bits<<std::endl<<std::endl;
  //Bits collected, now get the trigger type
  std::bitset<TypeSizes::TriggerWordSize> trigger_type_bits; 
  trigger_type_bits ^= (bits >> (TypeSizes::TriggerWordSize - 5));
  int trigger_type = static_cast<int>(trigger_type_bits.to_ulong());
  //If we have a muon trigger, get which trigger it was and increase the hit rate
  if (trigger_type == 16){
    std::bitset<TypeSizes::TriggerWordSize> muon_trigger_bits;
    //Shift the bits left by 5 first to remove the trigger type bits
    muon_trigger_bits ^= (bits << 5);
    //std::cout<<muon_trigger_bits<<std::endl;
    //Now we need to shift the entire thing set to the LSB so we can record the number
    //The muon trigger pattern words are 4 bits
    muon_trigger_bits >>= (TypeSizes::TriggerWordSize-4);
    int muon_trigger = static_cast<int>(muon_trigger_bits.to_ulong());
    fMuonTriggerRates[muon_trigger]++;
  }
  return;
}

void OnlineMonitoring::PTBFormatter::ReverseBits(std::bitset<TypeSizes::TriggerWordSize> &bits){
  for (unsigned int i = 0; i < bits.size()/2; i++){
    bool bit_up = bits[i];
    bool bit_down = bits[bits.size()-1-i];
    bits[i] = bit_down;
    bits[bits.size()-1-i] = bit_up;
  }
}

void OnlineMonitoring::PTBFormatter::ReverseBits(std::bitset<TypeSizes::CounterWordSize> &bits){
  for (unsigned int i = 0; i < bits.size()/2; i++){
    bool bit_up = bits[i];
    bool bit_down = bits[bits.size()-1-i];
    bits[i] = bit_down;
    bits[bits.size()-1-i] = bit_up;
  }
}

void OnlineMonitoring::PTBFormatter::ReverseBits(std::bitset<8> &bits){
  for (unsigned int i = 0; i < bits.size()/2; i++){
    bool bit_up = bits[i];
    bool bit_down = bits[bits.size()-1-i];
    bits[i] = bit_down;
    bits[bits.size()-1-i] = bit_up;
  }
}
