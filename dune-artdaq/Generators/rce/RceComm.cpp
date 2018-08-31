#include "RceComm.hh"

#include <iostream>
#include <sstream>

#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>

using boost::asio::ip::tcp;
using boost::lambda::bind;
using boost::lambda::var;

namespace dune {
namespace rce {

RceComm::RceComm(const std::string& host, short port, size_t timeout) :
   _deadline (_io_context),
   _socket   (_io_context),
   _timeout  (boost::posix_time::milliseconds(timeout))
{

   _deadline.expires_at(boost::posix_time::pos_infin);
   _check_deadline();

   // Connect to RCE
   auto endpoints = tcp::resolver(_io_context).resolve(host, std::to_string(port));

   _deadline.expires_from_now(_timeout);

   boost::system::error_code ec = boost::asio::error::would_block;
   boost::asio::async_connect(_socket, endpoints, var(ec) = boost::lambda::_1);
  
   do _io_context.run_one(); while (ec == boost::asio::error::would_block);

   if (ec || !_socket.is_open())
      throw boost::system::system_error(
            ec ? ec : boost::asio::error::operation_aborted);

   flush();
}

void RceComm::_check_deadline()
{
   if (_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now()) {
      boost::system::error_code ignored_ec;
      _socket.close(ignored_ec);
      _deadline.expires_at(boost::posix_time::pos_infin);
    }

    _deadline.async_wait(bind(&RceComm::_check_deadline, this));
}

size_t RceComm::read(std::string& msg)
{
   _io_context.reset();
   _deadline.expires_from_now(_timeout);

   boost::system::error_code ec = boost::asio::error::would_block;
   std::size_t bytes = 0;

   boost::asio::streambuf streambuf;
   boost::asio::async_read_until(_socket, streambuf, '\f',
         boost::bind(&RceComm::_handle_rw, _1, _2, &ec, &bytes));

   do _io_context.run_one(); while (ec == boost::asio::error::would_block);

   if (ec)
      throw boost::system::system_error(ec);

   // convert streambuf to std::string
   msg = "";
   if (streambuf.size() > 0)
   {
      auto buf       = streambuf.data();
      auto buf_begin = boost::asio::buffers_begin(buf);
      auto buf_end   = buf_begin + streambuf.size() - 1;

      msg = std::string(buf_begin, buf_end);
   }
   return bytes;
}

size_t RceComm::read()
{
   std::string tmp;
   return this->read(tmp);
}

size_t RceComm::send(const std::string &msg)
{
   _io_context.reset();
   _deadline.expires_from_now(_timeout);

   boost::system::error_code ec = boost::asio::error::would_block;
   std::size_t bytes = 0;

   boost::asio::streambuf streambuf;
   boost::asio::async_write(_socket, boost::asio::buffer(msg),
         boost::bind(&RceComm::_handle_rw, _1, _2, &ec, &bytes));

   do _io_context.run_one(); while (ec == boost::asio::error::would_block);

   if (ec)
      throw boost::system::system_error(ec);

   return bytes;
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

void RceComm::blowoff_hls(unsigned int flag)
{
   send_cfg("DataDpm.DataDpmHlsMon.Blowoff", flag);
}

void RceComm::blowoff_wib(bool flag)
{
   send_cfg("DataDpm.DataDpmWib.Blowoff", flag ? "True" : "False");
}

void RceComm::set_readout(size_t duration, size_t pretrigger)
{

   boost::property_tree::ptree pt;
   pt.add("DataDpm.DataBuffer.Duration",   duration);
   pt.add("DataDpm.DataBuffer.PreTrigger", pretrigger);
   
   boost::property_tree::ptree cfg;
   cfg.add_child("system.config", pt);

   send(cfg);
   read();
}

RceStatus RceComm::get_status()
{
   send("<system><command><ReadStatus/></command></system>\n\f");

   std::string msg;
   std::this_thread::sleep_for(std::chrono::milliseconds(500));
   read(msg);

   return RceStatus(msg);
}

}} // namespace dune::rce
