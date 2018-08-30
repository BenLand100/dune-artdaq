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
     int         _rce_host_port;
     std::string _rce_xml_file ;
     std::string _rce_run_mode ;
     bool        _rce_feb_emul ;
     int         _partition    ;
     int         _duration     ;
     int         _pretrigger   ;
     int         _hls_mask     ;

     std::string _daq_host_addr;
     std::string _daq_host_port;
     bool        _enable_rssi  ;

     std::string _instance_name;

     artdaq::Fragment::fragment_id_t _frag_id;

     std::unique_ptr<rce::RceComm>      _rce_comm;
     std::unique_ptr<rce::RssiReceiver> _receiver;

     // stats
     class Stats
     {
        public:
           Stats();

           rce::RecvStats prev;
           float    evt_rate  = 0;
           float    data_rate = 0;
           float    avg_size  = 0;
           float    min_size  = 0;
           float    max_size  = 0;
           uint32_t rssi_drop = 0;
           uint32_t pack_drop = 0;
           uint32_t bad_hdrs  = 0;
           uint32_t bad_data  = 0;
           uint32_t overflow  = 0;
           bool     is_open   = false;
           boost::posix_time::ptime last_update;

           void track_size(size_t bytes);
           void reset_track_size();

        private:
           bool     _1st_frag  = true;
           uint32_t _min_size  = 0;
           uint32_t _max_size  = 0;
     };

     Stats _stats;
     void _update_stats();
     void _send_stats()    const;
     void _print_stats()   const;
     void _print_summary(const char*title) const;

     void _check_status();

     std::stringstream _debug_ss;
     std::string       _last_frag = "";
     size_t            _frag_cnt  = 0;

};
} // namespace dune

#endif /* dune_artdaq_Generators_TpcRceReceiver_hh */
