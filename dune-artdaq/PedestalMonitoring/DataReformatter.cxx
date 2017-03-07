////////////////////////////////////////////////////////////////////////
// File:   DataReformatter.cxx
// Author: Mike Wallbank (July 2015)
//
// Utility to provide methods for reformatting the raw online data
// into a structure which is easier to use for monitoring.
////////////////////////////////////////////////////////////////////////

#include "DataReformatter.hxx"

int fRun = 0, fSubrun = 0, fEventNumber = 0;


// RCE ----------------------------------------------------------------------------------------------------------------------------------------------------
PedestalMonitoring::RCEFormatter::RCEFormatter(art::Handle<artdaq::Fragments> const& rawRCE) {

  if (!rawRCE.isValid()) return;

  this->AnalyseADCs(rawRCE);

}

void PedestalMonitoring::RCEFormatter::AnalyseADCs(art::Handle<artdaq::Fragments> const& rawRCE) {

  fNRCEs = rawRCE->size();

  // Loop over fragments to make a map to save the order the frags are saved in
  std::map<unsigned int, unsigned int> tpcFragmentMap;
  for (unsigned int fragIt = 0; fragIt < rawRCE->size(); ++fragIt) {
    const artdaq::Fragment &fragment = ((*rawRCE)[fragIt]);
    unsigned int fragmentID = fragment.fragmentID(); //+ 100;
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
      dune::TpcMilliSliceFragment millisliceFragment(frag);

      // Number of microslices in millislice fragments
      auto nMicroSlices = millisliceFragment.microSliceCount();

      // hNumMicroslicesInMillislice->SetBinContent(fragmentID,nMicroSlices);

      for (unsigned int microIt = 0; microIt < nMicroSlices; ++microIt) {

	// Get the microslice
	std::unique_ptr <const dune::TpcMicroSlice> microslice = millisliceFragment.microSlice(microIt);
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

    ADCs.push_back(adcVector);

  } // channel loop

}
