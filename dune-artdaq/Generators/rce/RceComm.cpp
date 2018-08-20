#include "RceComm.hh"

#include <iostream>
#include <sstream>

using boost::asio::ip::tcp;

namespace dune {
namespace rce {

typedef boost::system::error_code ErrorCode;

RceComm::RceComm(const std::string& host, short port, size_t timeout) :
   _timer  (_io_context),
   _socket (_io_context)
{
   std::cout << port << " " << timeout << std::endl;

   tcp::resolver resolver(_io_context);
   auto endpoints = resolver.resolve(host, "8090");
   boost::asio::async_connect(_socket, endpoints,
         [this](boost::system::error_code ec, tcp::endpoint endpoint)
         {
         if (!ec) {
            std::cout << "Connected to " << endpoint << std::endl;
         }
         });
   _io_context.run();
   _io_context.reset();
   flush();
}

RceComm::~RceComm()
{
   boost::asio::post(_io_context, [this]() { _socket.close(); });
   //std::cout << "Closed" << std::endl;
}

std::string RceComm::read()
{
   boost::asio::streambuf streambuf;
   boost::asio::async_read_until( _socket, streambuf, '\f',
         [this](const ErrorCode &ec, size_t bytes)
         {
            if (!ec) {
            }
               std::cout << "Read " << bytes << " bytes" << std::endl;
         });
   _io_context.run();
   _io_context.reset();

   // convert streambuf to std::string
   if (streambuf.size() > 0)
   {
      auto buf       = streambuf.data();
      auto buf_begin = boost::asio::buffers_begin(buf);
      auto buf_end   = buf_begin + streambuf.size() - 1;

      return std::string(buf_begin, buf_end);
   }
   return "";
}

size_t RceComm::send(const std::string &msg)
{
   boost::asio::async_write(_socket, boost::asio::buffer(msg),
         [this](const ErrorCode& ec, size_t bytes)
         {
            if (!ec) {
               std::cout << "Sent " << bytes << " bytes" << std::endl;
            }
         });

   _io_context.run();
   _io_context.reset();

   return 0;
}

void RceComm::flush()
{
   this->send("\a\n");
   this->read();
}

size_t RceComm::send(const boost::property_tree::ptree &pt)
{
   using boost::property_tree::ptree;
   std::ostringstream xml_stream;
   boost::property_tree::xml_writer_settings<std::string> settings;
   boost::property_tree::xml_parser::write_xml_element(xml_stream, 
         ptree::key_type(), pt, -1, settings);

   xml_stream << "\n\f";

   return send(xml_stream.str());
}

void RceComm::set_emul(bool is_emul)
{
   std::string loopback = is_emul ? "True" : "False";
   std::string rx_pol   = is_emul ? "False" : "True";

   boost::property_tree::ptree pt;
   pt.add("system.config.DataDpm.DataDpmEmu.Loopback", loopback);
   pt.add("system.config.DataDpm.DataDpmWib.RxPolarity", rx_pol);

   send(pt);
   read();
}

void RceComm::reset(const std::string &xml_file)
{
   boost::property_tree::ptree pt;
   pt.add("system.command.HardReset", "");

   if (xml_file == "") {
      pt.add("system.command.SetDefaults", "");
   }
   else {
      pt.add("system.command.ReadXmlFile", xml_file);
   }

   pt.add("system.command.SoftReset", "");

   send(pt);
   read();
}

void RceComm::set_partition(short partition)
{
   send_cfg("DataDpm.DataDpmTiming.PdtsEndpointTgrp", partition);
}

void RceComm::enable_rssi(bool rssi)
{
   send_cfg("DataDpm.DataBuffer.EnableRssi", rssi ? "True" : "False");
}

void RceComm::set_run_mode(const std::string &mode)
{
   send_cfg("DataDpm.DataBuffer.RunMode", mode);
}

void RceComm::set_daq_host(const std::string &addr)
{
   send_cfg("DataDpm.DataBuffer.DaqHost", addr);
}

}} // namespace dune::rc
