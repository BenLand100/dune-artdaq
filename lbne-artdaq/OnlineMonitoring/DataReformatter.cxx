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
    HasData = false;
    return;
  }

  this->AnalyseADCs(rawRCE, scopeMode);
  this->Windowing();

  return;

}

void OnlineMonitoring::RCEFormatter::AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE, bool scopeMode) {

  /// Analyses the data within the artdaq::Fragment and puts it in a form which is easier to use

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
      int nLastNanoSlices = 0;

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	// Get the microslice
	std::unique_ptr<const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
	auto nNanoSlices = microslice->nanoSliceCount();

	// See if this is the first microslice with a payload
	if (nNanoSlices > 0 and nLastNanoSlices == 0)
	  FirstMicroslice = microIt;
	nLastNanoSlices = nNanoSlices;

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

void OnlineMonitoring::RCEFormatter::AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE, int firstMicroslice, int lastMicroslice) {

  /// Analyses only specific microslices within each millislice for use in the online event display

  // Loop over fragments to make a map to save the order the frags are saved in
  std::map<unsigned int, unsigned int> tpcFragmentMap;
  for (unsigned int fragIt = 0; fragIt < rawRCE->size(); ++fragIt) {
    const artdaq::Fragment &fragment = ((*rawRCE)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID();
    tpcFragmentMap.insert(std::pair<unsigned int, unsigned int>(fragmentID,fragIt));
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

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	if ( ! ((int)microIt >= firstMicroslice and (int)microIt <= lastMicroslice) )
	  continue;

	// Get the microslice
	std::unique_ptr<const lbne::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
	auto nNanoSlices = microslice->nanoSliceCount();

	for (unsigned int nanoIt = 0; nanoIt < nNanoSlices; ++nanoIt) {
	
	  // Get the ADC value
	  uint16_t adc = std::numeric_limits<uint16_t>::max();
	  bool success = microslice->nanosliceSampleValue(nanoIt, sample, adc);

	  if (success)
	    adcVector.push_back((int)adc);

	} // nanoslice loop

      } // microslice loop

    } // analyse fragment loop

    fEVDADCs.push_back(adcVector);

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
#ifdef OLD_CODE

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
  for (size_t idx = 0; idx < rawPTB->size(); ++idx) {
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
#else




////////////// Nuno's code ////////////////
// NFB: Init of static consts
const std::vector<lbne::PennMicroSlice::Payload_Trigger::trigger_type_t> OnlineMonitoring::PTBFormatter::fMuonTriggerTypes = {
    lbne::PennMicroSlice::Payload_Trigger::TD,
    lbne::PennMicroSlice::Payload_Trigger::TC,
    lbne::PennMicroSlice::Payload_Trigger::TB,
    lbne::PennMicroSlice::Payload_Trigger::TA };
const std::vector<lbne::PennMicroSlice::Payload_Trigger::trigger_type_t > OnlineMonitoring::PTBFormatter::fCalibrationTypes = {
    lbne::PennMicroSlice::Payload_Trigger::C4,
    lbne::PennMicroSlice::Payload_Trigger::C3,
    lbne::PennMicroSlice::Payload_Trigger::C2,
    lbne::PennMicroSlice::Payload_Trigger::C1 };




OnlineMonitoring::PTBFormatter::PTBFormatter(art::Handle<artdaq::Fragments> const& rawPTB) {

  if (!rawPTB.isValid()) {
    PTBData = false;
    return;
  }
  PTBData = true;

  // NFB: I don't understad the logic below. I replaced by one that is simpler to me, as I am not 
  // sure of the behaviour of iterators to bitfields.
  
  for (uint32_t i = 0; i < fMuonTriggerTypes.size(); ++i) 
    fMuonTriggerRates[fMuonTriggerTypes.at(i)] = 0;
  for (uint32_t i = 0; i < fCalibrationTypes.size(); ++i) 
    fCalibrationTriggerRates[fCalibrationTypes.at(i)] = 0;

  fSSPTriggerRates = 0;

  // Count the total ticks across the fragments
  unsigned long NTotalTicks = 0;

  // Loop over the fragments
  for (size_t idx = 0; idx < rawPTB->size(); ++idx) {

    // Grab the millislice fragment from the artdaq fragment
    const auto& frag((*rawPTB)[idx]);
    //std::cout << "Processing fragment " << idx << std::endl;


    lbne::PennMilliSliceFragment msf(frag);

    // Count the types of each payload in the millislice
    // Actually, why are these needed?
    // Use the get_next_payload()
    lbne::PennMilliSlice::Header::payload_count_t n_frames, n_frames_counter, n_frames_trigger, n_frames_timestamp;
    n_frames = msf.payloadCount(n_frames_counter, n_frames_trigger, n_frames_timestamp);
    
    // std::cout << "Reporting " << n_frames << " frames (C,T,TS) = (" << n_frames_counter << ", " 
    // 	      << n_frames_trigger << ", " << n_frames_timestamp << ")" << std::endl;
    

    // Add on the total number of ticks in this millislice
    NTotalTicks += msf.widthTicks();
    fNTotalTicks += msf.widthTicks();
    
    //std::cout << "Fragment has " << NTotalTicks << " ticks (" <<  fNTotalTicks << " total)"<< std::endl;
    

    lbne::PennMicroSlice::Payload_Header*    word_header = nullptr;
    lbne::PennMicroSlice::Payload_Counter*   word_p_counter = nullptr;
    lbne::PennMicroSlice::Payload_Timestamp* previous_timestamp = nullptr;
    lbne::PennMicroSlice::Payload_Header*    future_timestamp_header = nullptr;
    lbne::PennMicroSlice::Payload_Timestamp* future_timestamp = nullptr;
    lbne::PennMicroSlice::Payload_Trigger*   word_p_trigger = nullptr;
    uint8_t* payload_data = nullptr;
    uint32_t payload_index = 0;

    // uint16_t counter, trigger, timestamp, payloadCount;
    // payloadCount = msf.payloadCount(counter, trigger, timestamp);
    // std::cout << "Number of payloads is " << payloadCount << ", of which " << counter << " are counters, " << trigger << " are triggers and " << timestamp << " are timestamps" << std::endl;
    uint32_t *data = nullptr;
    //while (payload_index < (uint32_t)payloadCount-1) {
    //while (payload_data != nullptr) {
    do {

      payload_data = msf.get_next_payload(payload_index, word_header);
      if ((payload_data == nullptr ) || payload_index == n_frames) {
	//std::cout << " Returned NULL at index " << payload_index << std::endl;
	continue;
      }
      //std::cout << "Payload index " << payload_index << std::endl;

      fPayloadTypes.push_back(word_header->data_packet_type);

      // Switch with word type
      switch (word_header->data_packet_type) {

      // Counter
      case lbne::PennMicroSlice::DataTypeCounter:
	//std::cout << "It's a counter!" << std::endl;
	// cast the returned payload into a counter structure and parse it
	// Why are the counter bits necessary? Not going to collect them for now
	// Need to be careful with the times...should collect full timestamps
	// but those should always be calculated from a timestamp word
	word_p_counter = reinterpret_cast<lbne::PennMicroSlice::Payload_Counter*>(payload_data);
	data = reinterpret_cast<uint32_t*>(payload_data);
	// std::cout << std::bitset<32>(data[3]) << " " 
	// 	  << std::bitset<32>(data[2]) << " " 
	// 	  << std::bitset<32>(data[1]) << " " 
	// 	  << std::bitset<32>(data[0]) << " " << std::endl;

	// Collect the counter bits
	// FIXME: This is incredibly inefficient as it stores everything in memory. The amount of data can become quite big. I can imagine this causing troubles on the machine running the monitoring in the long term. Ideally this would be better to be calculated on-the-fly, otherwise it will mean trouble in the future. For now leave it as it was.
	fCounterWords.push_back(*word_p_counter);

	// NFB: This is risky as, unlike microslices, nothing ensures that a timestamp word is nearby
	// The best procedure is probably to byte the bullet and transfer the full timestamps with each frame/word
	// For now use this makeshift logic
	if (previous_timestamp != nullptr)
	  fCounterTimes.push_back(word_header->get_full_timestamp_post(previous_timestamp->nova_timestamp));
	else if (future_timestamp != nullptr)
	  fCounterTimes.push_back(word_header->get_full_timestamp_pre(future_timestamp->nova_timestamp));
	else {
	  // Find the closest timestamp in the future
	  future_timestamp = reinterpret_cast<lbne::PennMicroSlice::Payload_Timestamp*>(msf.get_next_timestamp(future_timestamp_header));
	  if (future_timestamp == nullptr) {
	    // This should never happen, but if it does the fragment is useless.
	    //std::cout  << "Can't find PTB timestamp words in millislice fragment! Logic will fail" << std::endl;
	    //mf::LogError("Monitoring") << "Can't find PTB timestamp words in millislice fragment! Logic will fail" << std::endl;
	    return;
	  }
	  else
	    fCounterTimes.push_back(word_header->get_full_timestamp_pre(future_timestamp->nova_timestamp));
	}
	break;

      // Trigger
      case lbne::PennMicroSlice::DataTypeTrigger:
	//std::cout << "It's a trigger!" << std::endl;
	//std::cout << std::bitset<32>(*reinterpret_cast<uint32_t*>(payload_data));
	word_p_trigger = reinterpret_cast<lbne::PennMicroSlice::Payload_Trigger*>(payload_data);
	CollectTrigger(word_p_trigger);
	break;

      // Timestamp
      case lbne::PennMicroSlice::DataTypeTimestamp:
	previous_timestamp = reinterpret_cast<lbne::PennMicroSlice::Payload_Timestamp*>(payload_data);
	//std::cout << "It's a timestamp!  (" <<  previous_timestamp->nova_timestamp << ")"<< std::endl;
	break;

      default:
	//std::cout << "This is a " << std::bitset<3>(word_header->data_packet_type) << std::endl;
	// do nothing
	break;
	
      }
      
    } while (payload_data != nullptr); // loop over payload

  } // loop over fragments

  // Total time of the event (in [s])
  fTimeSliceSize = NNanoSecondsPerNovaTick * NTotalTicks / (1e9);

  //std::cout << "Total ticks " << NTotalTicks << " and that makes total event length " << fTimeSliceSize << std::endl;

  return;

}

void OnlineMonitoring::PTBFormatter::AnalyzeCounter(uint32_t counter_index, lbne::PennMicroSlice::Payload_Timestamp::timestamp_t& activation_time, double& hit_rate) const {

  // This should not be done on a global level.
  // Better to do for all the counters at once, no?

  // NFB: Where is counter_index coming from? What does it actually mean?

  // We need to loop through the requested counter to check
  //   A) when it switched back on
  //   B) How many times it switched on

  // Initialize the values to be returned
  activation_time = 0.;
  hit_rate = 0;

  bool counter_previously_on = true;
  for (uint32_t pos = 0; pos < fCounterWords.size(); ++pos) {
    bool counter_currently_on = fCounterWords.at(pos).get_counter_status(counter_index);
    if (counter_previously_on && counter_currently_on)
      continue;
    else if (counter_previously_on && !counter_currently_on)
      counter_previously_on = false;
    else if (!counter_previously_on && !counter_currently_on)
      continue;
    else if (!counter_previously_on && counter_currently_on) {
      // Counter has switched on!
      counter_previously_on = true;
      // Get the time that the counter switched on
      lbne::PennMicroSlice::Payload_Timestamp::timestamp_t current_counter_time = fCounterTimes.at(pos);
      ++hit_rate;
      if (hit_rate==1)
        activation_time = current_counter_time;
    }
    else
      mf::LogWarning("Monitoring") << "Error in PTBReformatter logic" << std::endl;

  }

  hit_rate /= fTimeSliceSize;
  return;

}

double OnlineMonitoring::PTBFormatter::AnalyzeMuonTrigger(lbne::PennMicroSlice::Payload_Trigger::trigger_type_t trigger_number) const {

  /// Returns the trigger rate for the requested muon trigger

  double trigger_rate = fMuonTriggerRates.at(trigger_number);
  trigger_rate /= fTimeSliceSize;

  return trigger_rate;

}

double OnlineMonitoring::PTBFormatter::AnalyzeCalibrationTrigger(lbne::PennMicroSlice::Payload_Trigger::trigger_type_t trigger_number) const {

  /// Returns the trigger rate for the requested calibration trigger

  double trigger_rate = fCalibrationTriggerRates.at(trigger_number);
  trigger_rate /= fTimeSliceSize;

  return trigger_rate;

}

double OnlineMonitoring::PTBFormatter::AnalyzeSSPTrigger() const {

  /// Returns the trigger rate for the SSP trigger

  double trigger_rate = fSSPTriggerRates / (double)fTimeSliceSize;
  return trigger_rate;

}

void OnlineMonitoring::PTBFormatter::CollectTrigger(lbne::PennMicroSlice::Payload_Trigger *trigger) {

  /// Takes the trigger payload and analyses it, counting each specific trigger

  // Possible to have more than one trigger per word -- ask for each individually
  if (trigger->has_muon_trigger()) {
    for (uint32_t i = 0; i < fMuonTriggerTypes.size(); ++i) {
      if (trigger->has_muon_trigger(fMuonTriggerTypes.at(i))) {
	//std::cout << "Collecting trigger: " << std::bitset<4>(fMuonTriggerTypes.at(i)) << std::endl;
        fMuonTriggerRates[fMuonTriggerTypes.at(i)]++;
      }
    }
  }
  if (trigger->has_calibration()) {
    for (uint32_t i = 0; i < fCalibrationTypes.size(); ++i) {
      if (trigger->has_calibration(fCalibrationTypes.at(i))) {
	//std::cout << "Collecting calibration: " << std::bitset<4>(fCalibrationTypes.at(i)) << std::endl;
        fCalibrationTriggerRates[fCalibrationTypes.at(i)]++;
      }
    }
  }

  if (trigger->has_ssp_trigger())
    ++fSSPTriggerRates;
  
  // NFB: What about RCE triggers? Are they no longer needed?
  // MW: Yes, they're needed. Haven't put them in yet! I'm sure your new code will make that easier.


  return;

}

#endif

