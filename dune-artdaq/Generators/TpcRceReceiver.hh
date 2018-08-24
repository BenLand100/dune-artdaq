#ifndef dune_artdaq_Generators_TpcRceReceiver_hh
#define dune_artdaq_Generators_TpcRceReceiver_hh

#include <memory>
#include <sstream>

#include "fhiclcpp/fwd.h"
#include "artdaq-core/Data/Fragment.hh"
#include "artdaq/Application/CommandableFragmentGenerator.hh"

#include "dune-artdaq/Generators/rce/RceComm.hh"
#include "dune-artdaq/Generators/rce/RceRssiReceiver.hh"

namespace dune {

namespace rce {
class BRStats
{
   public:
      BRStats();

      boost::posix_time::ptime last_update ;
      std::string              last_frag   ;
      uint64_t                 bad_hdrs    ;
      uint64_t                 nfrags      ;
};
} //namespace rce

class TpcRceReceiver : public artdaq::CommandableFragmentGenerator
{
   public:
      explicit TpcRceReceiver(fhicl::ParameterSet const& ps);

   //protected:
   //   std::string metricsReportingInstanceName() const {
   //      return _metric_instance_name;
   //   }

   private:
      // The "getNext_" function is used to implement user-specific
      // functionality; it's a mandatory override of the pure virtual
      // getNext_ function declared in CommandableFragmentGenerator
      bool getNext_(artdaq::FragmentPtrs& output) override;


     // JCF, Dec-11-2015

     // startOfDatataking will determine whether or not we've begun
     // taking data, either because of a new run (from the start
     // transition) or a new subrun (from the resume transition). It's
     // designed to be used to determine whether no fragments are
     // getting sent downstream (likely because of upstream hardware
     // issues)
     bool startOfDatataking();

     // State transition methods, for future use, if/when needed
     void start       () override  ;
     void stop        () override  ;
     void stopNoMutex () override  ;
     void pause       () override {}
     void resume      () override {}

     // Reporting functionality, for future use, if/when needed
     std::string report() override { return ""; }

     std::string _rce_host_addr;
     std::string _rce_host_port;
     std::string _rce_xml_file ;
     std::string _rce_run_mode ;
     bool        _rce_feb_emul ;
     int         _partition    ;

     std::string _daq_host_addr;
     std::string _daq_host_port;
     bool        _enable_rssi  ;

     std::string _instance_name;

     artdaq::Fragment::fragment_id_t _frag_id;

     std::unique_ptr<rce::RceComm>      _rce_comm;
     std::unique_ptr<rce::RssiReceiver> _receiver;

     rce::BRStats _stats;
     rce::BRStats _stats_prev;

     void _print_stats();
     std::stringstream _debug_ss;
};
} // namespace dune

#endif /* dune_artdaq_Generators_TpcRceReceiver_hh */
