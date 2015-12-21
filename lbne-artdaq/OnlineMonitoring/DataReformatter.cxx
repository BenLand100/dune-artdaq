////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.cxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#include "DataReformatter.hxx"

// RCE ----------------------------------------------------------------------------------------------------------------------------------------------------
OnlineMonitoring::RCEFormatter::RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE, bool scopeMode) {

  if (!rawRCE.isValid()) {
    NumRCEs = 0;
    return;
  }

  this->AnalyseADCs(rawRCE, scopeMode);
  this->Windowing();

  return;

}

void OnlineMonitoring::RCEFormatter::AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE, bool scopeMode) {

  NumRCEs = rawRCE->size();
  HasData = false;

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

    if (scopeMode and chanIt > 0)
      break;

    // Vector of ADC values for this channel
    std::vector<int> adcVector;
    //And the timestamps
    std::vector<unsigned long> timestampVector;

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

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	// Get the microslice
	std::unique_ptr <const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
	auto nNanoSlices = microslice->nanoSliceCount();

	// Get the channel for scope mode
	lbne::TpcMicroSlice::Header::softmsg_t us_software_message = microslice->software_message();
	fScopeChannel = uint32_t((us_software_message)& 0xFFFFFFFF);

	for (unsigned int nanoIt = 0; nanoIt < nNanoSlices; ++nanoIt) {
	
	  // Get the ADC value
	  uint16_t adc = std::numeric_limits<uint16_t>::max();
	  bool success = microslice->nanosliceSampleValue(nanoIt, sample, adc);

	  if (success) {
	    if ((int)adc > 10) HasData = true;
	    adcVector.push_back((int)adc);
	    //unsigned long timestamp = microslice->nanosliceNova_timestamp(nanoIt);
	    unsigned long timestamp = 1000;
	    timestampVector.push_back(timestamp);
	  }

	} // nanoslice loop

      } // microslice loop

    } // analyse fragment loop

    fADCs.push_back(adcVector);
    fTimestamps.push_back(timestampVector);

  } // channel loop

  return;

}

void OnlineMonitoring::RCEFormatter::Windowing() {

  const int fWindowingZeroThresholdSigned = 10;
  int fWindowingNearestNeighbour = 4;

  for (int channel = 0; channel < (int)fADCs.size(); ++channel) {

    // Define variables for the windowing
    std::vector<short> tmpBlockBegin((fADCs.at(channel).size()/2)+1);
    std::vector<short> tmpBlockSize((fADCs.at(channel).size()/2)+1);
    int tmpNumBlocks = 0;
    int blockstartcheck = 0;
    int endofblockcheck = 0;

    for (int tick = 0; tick < (int)fADCs.at(channel).size(); ++tick) {
      // Windowing code (taken from raw.cxx::ZeroSuppression)
      if (blockstartcheck == 0) {
	if (fADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
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
	if (fADCs.at(channel).at(tick) > fWindowingZeroThresholdSigned) {
	  ++tmpBlockSize[tmpNumBlocks];
	  endofblockcheck = 0;
	}
	else {
	  if (endofblockcheck < fWindowingNearestNeighbour) {
	    ++endofblockcheck;
	    ++tmpBlockSize[tmpNumBlocks];
	  }
	  //block has ended
	  else if ( (tick+1 < (int)fADCs.at(channel).size() and std::abs(fADCs.at(channel).at(tick+1)) <= fWindowingZeroThresholdSigned) or
		    (tick+2 < (int)fADCs.at(channel).size() and std::abs(fADCs.at(channel).at(tick+2)) <= fWindowingZeroThresholdSigned) ) {  
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

  return;

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

  return;

}


// PTB ----------------------------------------------------------------------------------------------------------------------------------------------------
OnlineMonitoring::PTBFormatter::PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB, PTBTrigger const& previousTrigger) {

  if (!rawPTB.isValid()) {
    PTBData = false;
    return;
  }
  PTBData = true;

  fPreviousTrigger = previousTrigger;

  // Initialise the trigger rates (both muon and calibration triggers have the same types)
  std::vector<unsigned int> trigger_types = {1,2,4,8};
  for (std::vector<unsigned int>::iterator triggerType = trigger_types.begin(); triggerType != trigger_types.end(); ++triggerType) {
    fMuonTriggerRates[*triggerType] = 0;
    fCalibrationTriggerRates[*triggerType] = 0;
  }

  unsigned long NTotalTicks = 0;

  //Loop over the generic fragments
  for (size_t idx = 0; idx < rawPTB->size(); ++idx){
    const auto& frag((*rawPTB)[idx]);

    //Get the PennMilliSliceFragment from the artdaq fragment
    lbne::PennMilliSliceFragment msf(frag);

    //Get the number of each payload type in the millislice
    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = msf.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);

    //Add on the total number of ticks in this millislice
    NTotalTicks += msf.widthTicks();
    fNTotalTicks += msf.widthTicks();

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
      fPayloadTypes.push_back(payload_type);
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
          CollectMuonTrigger(payload_data, payload_size, timestamp);
          fMuonTriggerTimes.push_back(timestamp);
        default:
          break;
      }
    }
  }
  //Now calculate what the total time of the event is in seconds
  fTimeSliceSize = NNanoSecondsPerNovaTick * NTotalTicks / (1000*1000*1000);

  return;
}

void OnlineMonitoring::PTBFormatter::AnalyzeCounter(int counter_index, unsigned long& activation_time, double& hit_rate) const {

  //We need to loop through the requested counter to check
  //  A) when it switched back on
  //  B) How many times it switched on
  //For now, we are going to ingore the counter if it begins the millislice activated UNTIL it switches off
  //
  //New addition: ignore whatever a counter does until a specifc time has passed (defined to be 400 ns) nominally

  //Initialise the values to be returned
  activation_time = 0.;
  hit_rate = 0;
  //Assume that the counter was on in the previous millislice (this shouldn't be detrimental to the logic)
  bool counter_previously_on = true;

  //The time that the counter switched on.  Used for checking the ignore time
  unsigned long last_counter_time = 0;

  for (std::vector<std::bitset<TypeSizes::CounterWordSize> >::const_iterator countIt = fCounterBits.begin(); countIt != fCounterBits.end(); countIt++){

    //Get the counter status
    bool counter_currently_on = (*countIt)[counter_index];

    if (std::distance(fCounterBits.begin(),countIt) == 0 && counter_currently_on){
      last_counter_time = fCounterTimes[std::distance(fCounterBits.begin(),countIt)];
      continue;
    }
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
      //Get the time that the counter switched on.
      int index = std::distance(fCounterBits.begin(), countIt);
      unsigned long current_counter_time = fCounterTimes.at(index);
      if (current_counter_time-last_counter_time > PTBHitIgnoreTime){
        hit_rate++;
	last_counter_time = current_counter_time;
        //Also record the activation time if it is the first time the counter has been hit
        if (hit_rate==1){
          //We need the array index to fetch the timestamp of the payload
          activation_time = current_counter_time;
        }
      }
    }
    else{
      //We should never get here
      std::cout<<"ERROR IN PTBFORMATTER'S LOGIC IN COUNTER ANALYSIS"<<std::endl;
    }
  }

  hit_rate /= fTimeSliceSize;
  return;
}

void OnlineMonitoring::PTBFormatter::AnalyzeMuonTrigger(int trigger_number, double& trigger_rate) const {
  trigger_rate = fMuonTriggerRates.at(trigger_number);
  trigger_rate /= fTimeSliceSize;
  return;
}

void OnlineMonitoring::PTBFormatter::AnalyzeCalibrationTrigger(int trigger_number, double& trigger_rate) const {
  trigger_rate = fMuonTriggerRates.at(trigger_number);
  trigger_rate /= fTimeSliceSize;
  return;
}

void OnlineMonitoring::PTBFormatter::AnalyzeSSPTrigger(double& trigger_rate) const {
  trigger_rate = fSSPTriggerRates / (double)fTimeSliceSize;
  return;
}

//   //
//   //NEW CODE WITH IGNORE BIT CHECKING
//   //We need to loop through the requested trigger to check
//   //  A) when it switched back on
//   //  B) How many times it switched on
//   //For now, we are going to ingore the trigger if it begins the millislice activated UNTIL it switches off
//   //
//   //New addition: ignore whatever a trigger does until a specifc time has passed (defined to be 400 ns) nominally

//   //Initialise the values to be returned
//   //activation_time = 0.;
//   trigger_rate = 0;
//   //Assume that the trigger was on in the previous millislice (this shouldn't be detrimental to the logic)
//   bool trigger_previously_on = true;

//   for (std::vector<std::bitset<TypeSizes::TriggerWordSize> >::const_iterator triggerIt = fMuonTriggerBits.begin(); triggerIt != fMuonTriggerBits.end(); triggerIt++){

//     //Get the trigger status
//     bool trigger_currently_on = (*triggerIt)[trigger_number];

//     if (trigger_previously_on && trigger_currently_on){
//       //If the trigger was previously on and it is still on, just continue
//       continue;
//     }
//     else if (trigger_previously_on && !trigger_currently_on){
//       //The trigger WAS on but it is now off so record the trigger as switching off and move on
//       trigger_previously_on = false;
//       continue;
//     }
//     else if (!trigger_previously_on && !trigger_currently_on){
//       //The trigger was off and it is still off so very little to do here.  Continue!
//       continue;
//     }
//     else if (!trigger_previously_on && trigger_currently_on){
//       //The trigger has switched on!!!!!
//       //Record that it is now on and record some numbers
//       trigger_previously_on = true;
//       ++trigger_rate;
//     }
//     else{
//       //We should never get here
//       std::cout<<"ERROR IN PTBFORMATTER'S LOGIC IN COUNTER ANALYSIS"<<std::endl;
//     }
//   }

//   trigger_rate /= fTimeSliceSize;

//   return;
// }

void OnlineMonitoring::PTBFormatter::CollectCounterBits(uint8_t* payload, size_t payload_size) {
  std::bitset<TypeSizes::CounterWordSize> bits;
  for (size_t ib = 0; ib < payload_size; ++ib){
    std::bitset<TypeSizes::CounterWordSize> byte = payload[ib];
    bits ^= (byte << 8*(payload_size-(ib+1)));
  }

  fCounterBits.push_back(bits);
  return;
}

void OnlineMonitoring::PTBFormatter::CollectMuonTrigger(uint8_t* payload, size_t payload_size, lbne::PennMicroSlice::Payload_Header::short_nova_timestamp_t timestamp) {
  std::bitset<TypeSizes::TriggerWordSize> bits;
  for (size_t ib = 0; ib < payload_size; ib++){
    std::bitset<TypeSizes::TriggerWordSize> byte = payload[ib];
    // bits ^= (byte << 8*ib);
    bits ^= (byte << 8*(payload_size-(ib+1)));
  }

  // Find out if this trigger is the same as the previous trigger
  if (timestamp - fPreviousTrigger.first < 25 and
      bits == fPreviousTrigger.second)
    return;

  // Bits collected, now get the trigger type
  std::bitset<TypeSizes::TriggerWordSize> trigger_type_bits; 
  trigger_type_bits ^= (bits >> (TypeSizes::TriggerWordSize - 5));
  int trigger_type = static_cast<int>(trigger_type_bits.to_ulong());

  // Get the triggers and collect the bits

  if (trigger_type == 16) {
    // Muon trigger
    std::bitset<TypeSizes::TriggerWordSize> muon_trigger_bits;
    //Shift the bits left by 5 first to remove the trigger type bits
    muon_trigger_bits ^= (bits << 5);
    //Now we need to shift the entire thing set to the LSB so we can record the number
    //The muon trigger pattern words are 4 bits
    muon_trigger_bits >>= (TypeSizes::TriggerWordSize-4);
    int muon_trigger = static_cast<int>(muon_trigger_bits.to_ulong());
    fMuonTriggerRates[muon_trigger]++;
    fMuonTriggerBits.push_back(muon_trigger_bits);
  }
  else if (trigger_type == 0) {
    // Calibration trigger
    std::bitset<TypeSizes::TriggerWordSize> calibration_trigger_bits;
    //Shift the bits left by 5 first to remove the trigger type bits
    calibration_trigger_bits ^= (bits << 5);
    //Now we need to shift the entire thing set to the LSB so we can record the number
    //The calibration trigger pattern words are 4 bits
    calibration_trigger_bits >>= (TypeSizes::TriggerWordSize-4);
    int calibration_trigger = static_cast<int>(calibration_trigger_bits.to_ulong());
    fCalibrationTriggerRates[calibration_trigger]++;
    fCalibrationTriggerBits.push_back(calibration_trigger_bits);    
  }
  else if (trigger_type == 8) {
    // SSP trigger
    ++fSSPTriggerRates;
  }

  fPreviousTrigger = std::make_pair(timestamp, bits);

  return;

}
