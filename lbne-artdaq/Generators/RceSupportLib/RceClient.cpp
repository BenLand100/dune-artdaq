/*
 * RceClient.cpp
 *
 *  Created on: Jun 26, 2014
 *      Author: tcn45
 */

#include "RceClient.hh"
#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>

lbne::RceClient::RceClient(const std::string& instance_name, const std::string& host_name,
		const std::string& port_or_service, const unsigned int timeout_usecs) :
	instance_name_(instance_name),
	socket_(io_service_),
	deadline_(io_service_),
	timeout_usecs_(timeout_usecs)
{

	// Initialise deadline timer to positive infinity so that no action will be taken until a
	// deadline is set
	deadline_.expires_at(boost::posix_time::pos_infin);

	// Start the persistent deadline actor that checks for timeouts
	this->check_deadline();

	// Attempt to establish the connection to the RCE, using a timeout if necessary
	try
	{

		// Resolve possible endpoints from specified host and port/service
		tcp::resolver resolver(io_service_);
		tcp::resolver::iterator endpoint_iter = resolver.resolve(tcp::resolver::query(host_name, port_or_service));
		tcp::resolver::iterator end;

		while ((endpoint_iter != end) && (socket_.is_open() == false))
		{
			tcp::endpoint endpoint = *endpoint_iter++;
			mf::LogInfo(instance_name_) << "Connecting to RCE at " << endpoint;

			// If a client timeout is specified, set the deadline timer appropriately
			this->set_deadline();

			// Set error code to would_block as async operations guarantee not to set this value
			boost::system::error_code error = boost::asio::error::would_block;

			// Start the asynchronous connection attempt, which will call the appropriate handler
			// on completion
			socket_.async_connect(endpoint, boost::bind(&lbne::RceClient::async_connect_handler, _1, &error));

			// Run the IO service and block until the connection handler completes
			do
			{
				io_service_.run_one();
			}
			while (error == boost::asio::error::would_block);

			// If the deadline timer has been called and cancelled the socket operation before the
			// connection has established, a timeout occurred
			if (error == boost::asio::error::operation_aborted)
			{
				socket_.close();
				mf::LogError(instance_name_) << "Timeout establishing client connection to RCE at " << endpoint;
				// TODO replace with exception
			}
			// If another error occurred during connect - throw an exception
			else if (error)
			{
				socket_.close();
				mf::LogError(instance_name_) << "Error establishing connection to RCE at " << endpoint << " : " << error.message();
			}
		}
		if (socket_.is_open() == false)
		{
			mf::LogError(instance_name_) << "Failed to open connection to RCE";
		} else {
			// Send RCE a bell character to suppress async updates
			this->send("\a\n");
			// Flush the socket of any stale aysnc update data from RCE
			size_t bytesFlushed = 0;
			std::string data;
			do {
			  data = this->receive();
			  bytesFlushed += data.length();
			} while (data.length() > 0);
			mf::LogDebug(instance_name_)  << "Flushed " << bytesFlushed << " bytes of stale data from client socket";
		  
		}
	}
	catch (boost::system::system_error& e)
	{
		mf::LogError(instance_name_) << "Exception caught opening connection to RCE: " << e.what();
	}

}

lbne::RceClient::~RceClient()
{
	try {
		socket_.close();
	}
	catch (boost::system::system_error& e)
	{
		mf::LogError(instance_name_) << "Exception caught closing RceClient connection:" << e.what();
	}
}

void lbne::RceClient::send_command(std::string const & command, std::string const & param)
{

	mf::LogDebug(instance_name_) << "Sending command: " << command << " with param: " << param;
	
	// Build XML fragment containing command enclosing the parameter
	std::ostringstream xml_frag;
	xml_frag << "<command><" << command << ">";
	xml_frag << param;
	xml_frag << "</" << command << "></command>";

	// Send it
	this->send_xml(xml_frag.str());
}

void lbne::RceClient::send_command(std::string const & command)
{
	mf::LogDebug(instance_name_) << "Sending command: " << command;

	// Build the XML fragment with command as empty closed tag
	std::ostringstream xml_frag;
	xml_frag << "<command><" << command << "/></command>";

	// Send it
	this->send_xml(xml_frag.str());
}

void lbne::RceClient::send_config(std::string const & config)
{
	mf::LogDebug(instance_name_) << "Sending config: " << config;
	std::ostringstream config_frag;
	config_frag << "<config>" << config << "</config>";

	this->send_xml(config_frag.str());
}

void lbne::RceClient::send_xml(std::string const & xml_frag)
{

	// Wrap the XML fragment for this request in the root system tags
	std::ostringstream xml_cmd;
	xml_cmd << "<system>";
	xml_cmd << xml_frag ;
	xml_cmd << "</system>\n\f";

	// Send the XML request to the RCEcomman
	this->send(xml_cmd.str());

	// Get the response
	xmlDocPtr doc;
	std::string response;

	int max_response_timeout_us = 10000000;
	int max_retries = max_response_timeout_us / timeout_usecs_;
	int retries = 0;

	while ( retries++ < max_retries)
	{
	  response = this->receive();
	  doc = xmlReadMemory(response.c_str(), response.length()-1, "noname.xml", NULL, 0);

	  if (doc != NULL)
	  {
	    //std::cout << "Response is " << response << std::endl;	    
	    break;
	  }
	}

	if (doc == NULL) {
		mf::LogError(instance_name_) << "Failed to parse XML response (length: " <<  response.length() << ")";
		mf::LogDebug(instance_name_) << response;
	}
	else
	{

	  // Get the root element node
	  xmlNode* root_element = xmlDocGetRootElement(doc);
	  xmlNode *cur_node = NULL;

	  // Traverse the DOM of the XML response and determine if any of the child elements are error.
	  for (cur_node = root_element->children; cur_node; cur_node = cur_node->next)
	  {
	    if (cur_node->type == XML_ELEMENT_NODE)
	    {
	      if (std::string((const char*)(cur_node->name)) == "error") 
	      {
			//xmlNode* error_node = cur_node->children;
			xmlChar* errorContent = xmlNodeGetContent(cur_node);
			if (errorContent) {
				mf::LogError(instance_name_) << "Got error response from RCE: " << errorContent;
			} else {
				mf::LogError(instance_name_) << "Got error resposne from RCE but cannot parse content";
			}
			xmlFree(errorContent);
	      }
	    }
	  }
	  xmlFreeDoc(doc);
	}
}

// ------------ Private methods ---------------

void lbne::RceClient::set_deadline(void)
{
	// Set the deadline for the asynchronous write operation if the timeout is set, otherwise
	// revert back to +ve infinity to stall the deadline timer actor
	if (timeout_usecs_ > 0)
	{
		deadline_.expires_from_now(boost::posix_time::microseconds(timeout_usecs_));
	}
	else
	{
		deadline_.expires_from_now(boost::posix_time::pos_infin);
	}
}
void lbne::RceClient::check_deadline(void)
{

	// Handle deadline if expired
	if (deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now())
	{
		// Cancel pending socket operation
		socket_.cancel();

		// No longer an active deadline to set the expiry to positive inifinity
		deadline_.expires_at(boost::posix_time::pos_infin);
	}

	// Put the deadline actor back to sleep
	deadline_.async_wait(boost::bind(&lbne::RceClient::check_deadline, this));
}

void lbne::RceClient::async_connect_handler(const boost::system::error_code& ec, boost::system::error_code* output_ec)
{
	*output_ec = ec;
}

void lbne::RceClient::async_completion_handler(
		const boost::system::error_code &error_code, std::size_t length,
		boost::system::error_code* output_error_code, std::size_t* output_length)
{
	*output_error_code = error_code;
	*output_length = length;
}

std::size_t lbne::RceClient::send(std::string const & send_str)
{

	// Set the deadline to enable timeout on send if appropriate
	this->set_deadline();

	// Set up variables to receive the result of the async operation. The error code is set
	// to would_block as ASIO guarantees that its async calls never fail with this value - any
	// other value therefore indicates completion or failure
	std::size_t send_len = 0;
	boost::system::error_code error = boost::asio::error::would_block;

	// Start the async operation. The asyncCompletionHandler function is bound as a callback
	// to update the error and length variables
	boost::asio::async_write(socket_, boost::asio::buffer(send_str),
			boost::bind(&lbne::RceClient::async_completion_handler, _1, _2, &error, &send_len));

	// Block until the async operation has completed
	do
	{
		io_service_.run_one();
	}
	while (error == boost::asio::error::would_block);

	// Handle any error conditions arising during the async operation
	if (error == boost::asio::error::eof)
	{
		// Connection closed by peer
		mf::LogError(instance_name_) << "Connection closed by RCE";
	}
	else if (error == boost::asio::error::operation_aborted)
	{
		// Timeout signalled by deadline actor
		mf::LogError(instance_name_) << "Timeout sending message to RCE";
	}
	else if (error)
	{
		mf::LogError(instance_name_) << "Error sending message to RCE: " << error.message();
	}
	else if (send_len != send_str.size())
	{
		mf::LogError(instance_name_) << "Size mismatch when sending transaction: wrote " << send_len << " expected " << send_str.size();
	}

	return send_len;
}

// std::string lbne::RceClient::receive(void)
// {

// 	this->set_deadline();

// 	std::size_t receive_length = 0;
// 	boost::system::error_code error = boost::asio::error::would_block;

// 	boost::array<char, 128> raw_buffer;

// 	socket_.async_read_some(boost::asio::buffer(raw_buffer),
// 			boost::bind(&lbne::RceClient::async_completion_handler, _1, _2, &error, &receive_length));

// 	do
// 	{
// 		io_service_.run_one();
// 	}
// 	while (error == boost::asio::error::would_block);

// 	return std::string(raw_buffer.data(), receive_length);
// }

std::string lbne::RceClient::receive(void)
{
  this->set_deadline();
  std::size_t receive_length = 0;
  boost::system::error_code error = boost::asio::error::would_block;

  boost::asio::streambuf response;
  
  boost::asio::async_read_until(socket_, response, "\f",
		boost::bind(&lbne::RceClient::async_completion_handler, _1, _2, &error, &receive_length));

  do
  {
    io_service_.run_one();
  }
  while (error == boost::asio::error::would_block);

  boost::asio::streambuf::const_buffers_type bufs = response.data();
  return std::string(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + response.size());

}

void lbne::RceClient::set_param_(std::string const & name, std::string const & encoded_value, std::string const & type)
{
	// Encode the parameter into a command with argument list
	std::ostringstream msg;
	msg << "SET"
		<< " param=" << name
		<< " value=" << encoded_value
		<< " type="  << type << std::endl;

	// Send it
	this->send(msg.str());

	// Get the response
	std::string response = this->receive();

	// Parse the response to ensure the command was acknowledged
	if (!response_is_ack(response, "SET"))
	{
		mf::LogError(instance_name_) << "SET command failed: " << response;
	}

}

bool lbne::RceClient::response_is_ack(std::string const & response, std::string const & command)
{
	bool is_ack = true;
	std::vector<std::string> tokens;

	boost::split(tokens, response, boost::is_any_of(" "));
	is_ack = ((tokens[0] == "ACK") && (tokens[1] == command));

	return is_ack;
}
