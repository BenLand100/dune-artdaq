#ifndef RCECOMM_HH__
#define RCECOMM_HH__

#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace dune {
namespace rce {

class RceComm
{
   public:
      RceComm(
            const std::string &host,
            short port=8192,
            size_t timeout=500);
      ~RceComm();

      std::string read();
      size_t      send(const std::string &msg);
      size_t      send(const boost::property_tree::ptree &pt);
      void        flush();

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
      void reset         ( const std::string& xml_file="" );

   private:
      boost::asio::io_service      _io_context;
      boost::asio::deadline_timer  _timer;
      boost::asio::ip::tcp::socket _socket;
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
