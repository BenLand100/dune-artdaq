#include "lbne-artdaq/Generators/TPCMilliSliceSimulatorWithCopy.hh"
#include "lbne-artdaq/Overlays/MilliSliceWriter.hh"
#include "artdaq/Application/GeneratorMacros.hh"
#include "lbne-artdaq/Overlays/FragmentType.hh"
#include "messagefacility/MessageLogger/MessageLogger.h"

lbne::TPCMilliSliceSimulatorWithCopy::
TPCMilliSliceSimulatorWithCopy(fhicl::ParameterSet const & ps) :
  CommandableFragmentGenerator(ps), work_buffer_(THIRD_PARTY_BUFFER_SIZE_)
{
  int fragment_id = ps.get<int>("fragment_id");
  fragment_ids_.push_back(fragment_id);

  // the following parameters are part of faking the hardware readout
  number_of_microslices_to_generate_ =
    ps.get<uint32_t>("number_of_microslices_to_generate", 1);
  number_of_nanoslices_to_generate_ =
    ps.get<uint32_t>("number_of_nanoslices_to_generate", 1);
  number_of_values_to_generate_ =
    ps.get<uint32_t>("number_of_values_to_generate", 3);
  simulated_readout_time_usec_ =
    ps.get<uint32_t>("simulated_readout_time", 100000);
}

bool lbne::TPCMilliSliceSimulatorWithCopy::getNext_(artdaq::FragmentPtrs & frags)
{
  // returning false indicates that we are done with the events in
  // this subrun or run
  if (should_stop()) {
    return false;
  }

  // if we didn't read any data from the hardware (pretend), we simply
  // return an empty list
  uint32_t data_size = pretend_to_read_a_millislice_from_the_hardware();
  if (data_size == 0) {
    return true;
  }

  // find the start of the (pretend) third party buffer
  uint8_t* data_ptr = get_start_address_of_third_party_buffer();

  // create the artdaq::Fragment (more suitable constructors coming soon)
  std::unique_ptr<artdaq::Fragment> frag( artdaq::Fragment::FragmentBytes(data_size));
  frag->setSequenceID(ev_counter());
  frag->setFragmentID(fragmentIDs()[0]);
  frag->setUserType(lbne::detail::TPC);

  // copy the data into the fragment
  memcpy(frag->dataBeginBytes(), data_ptr, data_size);

  // add the fragment to the list
  frags.emplace_back(std::move (frag));

  // increment the event counter
  ev_counter_inc();

  return true;
}

uint32_t lbne::TPCMilliSliceSimulatorWithCopy::
pretend_to_read_a_millislice_from_the_hardware()
{
  usleep(simulated_readout_time_usec_);

  const uint16_t CHANNEL_NUMBER = 128;
  const uint16_t SAMPLE1 = 0x7800;
  const uint32_t MICROSLICE_BUFFER_SIZE = 512;
  const uint32_t NANOSLICE_BUFFER_SIZE = 128;
  std::shared_ptr<lbne::MicroSliceWriter> microslice_writer_ptr;
  std::shared_ptr<lbne::NanoSliceWriter> nanoslice_writer_ptr;

  uint16_t channel_number = CHANNEL_NUMBER;
  uint16_t sample_value = SAMPLE1;
  lbne::MilliSliceWriter millislice_writer(&work_buffer_[0], THIRD_PARTY_BUFFER_SIZE_);
  for (uint32_t udx = 0; udx < number_of_microslices_to_generate_; ++udx) {
    microslice_writer_ptr = millislice_writer.reserveMicroSlice(MICROSLICE_BUFFER_SIZE);
    if (microslice_writer_ptr.get() == 0) {
      mf::LogError("TPCMilliSliceSimulatorWithCopy")
        << "Unable to create microslice number " << udx;
    }
    else {
      for (uint32_t ndx = 0; ndx < number_of_nanoslices_to_generate_; ++ndx) {
        nanoslice_writer_ptr = microslice_writer_ptr->reserveNanoSlice(NANOSLICE_BUFFER_SIZE);
        if (nanoslice_writer_ptr.get() == 0) {
          mf::LogError("TPCMilliSliceSimulatorWithCopy")
            << "Unable to create nanoslice number " << ndx;
        }
        else {
          nanoslice_writer_ptr->setChannelNumber(channel_number++);
          for (uint32_t sdx = 0; sdx < number_of_values_to_generate_; ++sdx) {
            nanoslice_writer_ptr->addSample(sample_value++);
          }
        }
      }
    }
  }
  millislice_writer.finalize();
  return millislice_writer.size();
}

uint8_t* lbne::TPCMilliSliceSimulatorWithCopy::
get_start_address_of_third_party_buffer()
{
  return &work_buffer_[0];
}

DEFINE_ARTDAQ_COMMANDABLE_GENERATOR(lbne::TPCMilliSliceSimulatorWithCopy) 
