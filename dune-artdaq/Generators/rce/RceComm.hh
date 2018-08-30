#ifndef RCECOMM_HH__
#define RCECOMM_HH__

#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "RceStatus.hh"

namespace dune {
namespace rce {

class RceComm
{
   public:
      RceComm( const std::string& host,
               short              port=8090,
               size_t             timeout=500 );

      size_t read   ( std::string& msg);
      size_t read   ();
      size_t send   (const std::string &msg);
      size_t send   (const boost::property_tree::ptree &pt);
      void   flush();

      template<typename T>
      void send_cmd(const std::string &cmd, T const &arg);

      void send_cmd(const std::string &cmd) { send_cmd(cmd, "");}

      template<typename T>
      void send_cfg(const std::string &cfg, T const &arg);

      void enable_rssi   ( bool rssi                      );
      void set_emul      ( bool is_emul                   );
      void set_partition ( short partition                );
      void set_run_mode  ( const std::string &mode        );
      void set_daq_host  ( const std::string &addr        );
      void blowoff_hls   ( unsigned int flag              );
      void blowoff_wib   ( bool         flag              );
      void reset         ( const std::string& xml_file="" );
      void set_readout   ( size_t duration,  // in microseconds
                           size_t pretrigger              );

      RceStatus get_status();

   private:
      boost::asio::io_service          _io_context;
      boost::asio::deadline_timer      _deadline;
      boost::asio::ip::tcp::socket     _socket;
      boost::posix_time::time_duration _timeout;

      void _check_deadline();
      static void _handle_rw(
            const boost::system::error_code &ec, std::size_t bytes,
            boost::system::error_code *out_ec , std::size_t* out_bytes) {
         *out_ec    = ec;
         *out_bytes = bytes;
      }
};

template<typename T>
void RceComm::send_cmd(const std::string &cmd, T const &arg) {
   boost::property_tree::ptree pt;
   pt.add("system.command." + cmd, arg);

   send(pt);
   read();
}

template<typename T>
void RceComm::send_cfg(const std::string &cfg, T const &arg) {
   boost::property_tree::ptree pt;
   pt.add("system.config." + cfg, arg);

   send(pt);
   read();
}

}} //namespace dune::rce
#endif
