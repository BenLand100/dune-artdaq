
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

  std::unique_ptr<WIB> wib;

};

}
