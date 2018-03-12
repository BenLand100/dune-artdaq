
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "WIB/WIB.hh"

namespace wibdaq {

class WIBReader : public artdaq::CommandableFragmentGenerator {
public:
  WIBReader();
  // "initialize" transition
  explicit WIBReader(fhicl::ParameterSet const& ps);
  // "shutdown" transition
  virtual ~WIBReader();

private:
  // "start" transition
  void start() override;
  // "stop" transition
  void stop() override;
  bool getNext_(artdaq::FragmentPtrs& output) override;
  void stopNoMutex() override {}

  void setupFEMBFakeData(size_t iFEMB, fhicl::ParameterSet const& FEMB_config);
  void setupFEMB(size_t iFEMB, uint32_t gain, uint32_t shape, uint32_t base, uint32_t leakHigh, uint32_t leak10X, uint32_t acCouple, uint32_t buffer, uint32_t tstIn, uint32_t extClk, uint8_t clk_cs, uint8_t pls_cs, uint8_t dac_sel, uint8_t fpga_dac, uint8_t asic_dac, uint8_t mon_cs, uint32_t expected_femb_fw_version);

  std::unique_ptr<WIB> wib;

};

}
