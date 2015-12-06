/*
 * PennClient.cpp
 *
 *  Created on: Dec 15, 2014
 *      Author: tdealtry (based on tcn45 rce code)
 */

#include "PennClient.hh"
#include "lbne-artdaq/DAQLogger/DAQLogger.hh"

#include <iostream>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <libxml/parser.h>
#include <libxml/tree.h>

//#define __PTB_DEVEL_MODE__

lbne::PennClient::PennClient(const std::string& host_name, const std::string& port_or_service, const unsigned int timeout_usecs) :
	socket_(io_service_),
	deadline_(io_service_),
	timeout_usecs_(timeout_usecs)
{
  DAQLogger::LogDebug("PennClient") << "lbne::PennClient constructor";

	// Initialise deadline timer to positive infinity so that no action will be taken until a
	// deadline is set
	deadline_.expires_at(boost::posix_time::pos_infin);

	// Start the persistent deadline actor that checks for timeouts
	this->check_deadline();

	// Attempt to establish the connection to the PENN, using a timeout if necessary
	try
	{

		// Resolve possible endpoints from specified host and port/service
		tcp::resolver resolver(io_service_);
		tcp::resolver::iterator endpoint_iter = resolver.resolve(tcp::resolver::query(host_name, port_or_service));
		tcp::resolver::iterator end;

		while ((endpoint_iter != end) && (socket_.is_open() == false))
		{
			tcp::endpoint endpoint = *endpoint_iter++;
			DAQLogger::LogInfo("PennClient") << "Connecting to PTB at " << endpoint;

			// If a client timeout is specified, set the deadline timer appropriately
			this->set_deadline();

			// Set error code to would_block as async operations guarantee not to set this value
			boost::system::error_code error = boost::asio::error::would_block;

			// Start the asynchronous connection attempt, which will call the appropriate handler
			// on completion
			socket_.async_connect(endpoint, boost::bind(&lbne::PennClient::async_connect_handler, _1, &error));

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
				DAQLogger::LogError("PennClient") << "Timeout establishing client connection to PTB at " << endpoint;
				// TODO replace with exception
			}
			// If another error occurred during connect - throw an exception
			else if (error)
			{
				socket_.close();

				// JCF, Jul-29-2015

				// Swallow exception thrown; does not necessarily prevent datataking
				try {
				  DAQLogger::LogError("PennClient") << "Error establishing connection to PTB at " << endpoint << " : " << error.message();
				} catch (...) {} // Swallow
			}
		}
		if (socket_.is_open() == false)
		{
			DAQLogger::LogError("PennClient") << "Failed to open connection to PENN";
		} else {

			// Flush the socket of any stale aysnc update data from PENN
			size_t bytesFlushed = 0;
			std::string data;
			do {
			  data = this->receive();
			  bytesFlushed += data.length();
			} while (data.length() > 0);
			DAQLogger::LogInfo("PennClient") << "Flushed " << bytesFlushed << " bytes of stale data from client socket";
		  
		}
	}
	catch (boost::system::system_error& e)
	{
		DAQLogger::LogError("PennClient") << "Exception caught opening connection to PENN: " << e.what();
	}

}

lbne::PennClient::~PennClient()
{
	try {
		socket_.close();
	}
	catch (boost::system::system_error& e)
	{
		DAQLogger::LogError("PennClient") << "Exception caught closing PennClient connection:" << e.what();
	}
}

void lbne::PennClient::send_command(std::string const & command, std::string & answer)
{

	DAQLogger::LogInfo("PennClient") << "Sending command: " << command << " expecting an answer.";
	
	// Build XML fragment containing command enclosing the parameter
	std::ostringstream xml_frag;
	xml_frag << "<command>" << command;
	xml_frag << "</command>";

	// Send it
	this->send_xml(xml_frag.str(), answer);
}

void lbne::PennClient::send_command(std::string const & command)
{
	DAQLogger::LogInfo("PennClient") << "Sending command: " << command;

	// Build the XML fragment with command as empty closed tag
	std::ostringstream xml_frag;
	xml_frag << "<command>" << command << "</command>";

	// Send it
	this->send_xml(xml_frag.str());
}

void lbne::PennClient::send_config(std::string const & config)
{
#ifdef __PTB_DEVEL_MODE__
	DAQLogger::LogDebug("PennClient") << "Sending config: " << config;
#endif

	std::string ptb_answer;
	this->send_xml(config,ptb_answer);

	// If anything else than the status is expected, it is returned by ptb_answer

//	std::ostringstream config_frag;
//	config_frag << "<config>" << config << "</config>";
//
//	this->send_xml(config_frag.str());
}

void lbne::PennClient::send_xml(std::string const & xml_frag) {
  std::map<std::string,std::string> dummy_answer;
  send_xml(xml_frag,dummy_answer);
}

void lbne::PennClient::send_xml(std::string const & xml_frag, std::string &xml_answer)
{

	// Wrap the XML fragment for this request in the root system tags
	std::ostringstream xml_cmd;
	xml_cmd << xml_frag ;

	// Send the XML request to the PTB
	this->send(xml_cmd.str());

	// Get the response
        xmlDocPtr doc;
	std::string response;

        int max_response_timeout_us = 10000000;
        int max_retries = max_response_timeout_us / timeout_usecs_;
        int retries = 0;

	while ( retries++ < max_retries) {
	  response = this->receive();
	  //	  doc = xmlReadMemory(response.c_str(), response.length()-1, "noname.xml", NULL, 0);
	  doc = xmlReadMemory(response.c_str(), response.length(), "noname.xml", NULL, 0);
	  if(doc != NULL) {
	    break;
	  }
	}//while(retries)

	// Traverse the DOM of the XML response and determine if any of the child elements are error.
	if (doc == NULL) {

	  // JCF, Jul-29-2015

	  // I'm swallowing the exception thrown by LogError here --
	  // in my experience, this issue doesn't prevent datataking

	  try { 
	    DAQLogger::LogError("PennClient") << "Failed to parse XML response: " << response <<  " (length " << response.length() << ")";
	  } catch (...) {} // Swallow
	}
	else {
	  /*Get the root element node */
	  xmlNode* root_element = xmlDocGetRootElement(doc);
	  xmlNode *cur_node = NULL;
	  for (cur_node = root_element->children; cur_node; cur_node = cur_node->next) {
	    if (cur_node->type == XML_ELEMENT_NODE) {
        if (std::string((const char*)(cur_node->name)) == "success")
        {
          xmlChar* msgContent = xmlNodeGetContent(cur_node);
          if (std::string((const char*)(msgContent)) == "true") {
            DAQLogger::LogInfo("PennClient") << "Command executed successfully on the PTB";
          } else {
            try{
            DAQLogger::LogError("PennClient") << "Command failed execution on the PTB";
            } catch (...) {} // Swallow
          }
          xmlFree(msgContent);
        }
        else if (std::string((const char*)(cur_node->name)) == "error")
	      {
	        //xmlNode* error_node = cur_node->children;
	        xmlChar* errorContent = xmlNodeGetContent(cur_node);
	        if (errorContent) {
	          DAQLogger::LogError("PennClient") << "Got error response from PTB: " << errorContent;
	        } else {
	          DAQLogger::LogError("PennClient") << "Got error response from PTB but cannot parse content";
	        }
	        xmlFree(errorContent);
	      }
        else if (std::string((const char*)(cur_node->name)) == "warning")
        {
          xmlChar* warnContent = xmlNodeGetContent(cur_node);
          if (warnContent) {
            DAQLogger::LogWarning("PennClient") << "Got warning response from PTB: " << warnContent;
          } else {
            DAQLogger::LogWarning("PennClient") << "Got warning response from PTB but cannot parse content";
          }
          xmlFree(warnContent);
        }
	      else if (std::string((const char*)(cur_node->name)) == "run_start") {
          xmlChar* answerContent = xmlNodeGetContent(cur_node);
          if (answerContent) {
            DAQLogger::LogInfo("PennClient") << "Got answer from the PTB : Run start time : " << answerContent;
            xml_answer = (const char*)answerContent;
          } else {
            DAQLogger::LogWarning("PennClient") << "Got run_start answer from PTB but cannot parse content";
          }
          xmlFree(answerContent);
	      } else {
	        DAQLogger::LogWarning("PennClient") << "Got an unrecognized answer from PTB with name " << (const char*)(cur_node->name);
	        xmlChar* answerContent = xmlNodeGetContent(cur_node);
	        if (answerContent) {
	          DAQLogger::LogWarning("PennClient") << " and content : " << answerContent;
	        } else {
	          DAQLogger::LogWarning("PennClient") << " and content unknown content.";
	        }
	      }
	    } else if (cur_node->type == XML_ATTRIBUTE_NODE) {
	      if (std::string((const char*)(cur_node->name)) == "run_statistics") {
	        std::vector<std::string> attributes;
	        attributes.push_back(std::string("num_microslices"));
	        attributes.push_back(std::string("num_word_counter"));
          attributes.push_back(std::string("num_word_trigger"));
          attributes.push_back(std::string("num_word_tstamp"));
          attributes.push_back(std::string("bytes_sent"));
          attributes.push_back(std::string("time_run_board"));
          std::ostringstream msg;
          msg << "PTB end of run statistics : \n";



          // -- Grab the different attributes
          xmlChar *prop_val = NULL;
          for (std::vector<std::string>::iterator it = attributes.begin(); it != attributes.end(); ++it) {

            prop_val = xmlGetProp(cur_node,(const xmlChar*)it->c_str());
            msg << " [" << *it << "] : " << (const char*)prop_val << "\n";
            xmlFree(prop_val);

          }
          // -- Statistics node usually sent at the end of the run
          // Just print them as an INFO message
          DAQLogger::LogInfo("PennClient") << msg.str();

	      } else {
	        DAQLogger::LogWarning("PennClient") << "Received unrecognized attribute type answer from PTB : [" << cur_node->name << "]";
	      }
	    } else {
        DAQLogger::LogWarning("PennClient") << "Received unrecognized XML element type from PTB : [" << cur_node->type << "]";
      }
	  }
	  xmlFreeDoc(doc);
	}//doc != NULL
}

// ------------ Private methods ---------------

void lbne::PennClient::set_deadline(void)
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
void lbne::PennClient::check_deadline(void)
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
	deadline_.async_wait(boost::bind(&lbne::PennClient::check_deadline, this));
}

void lbne::PennClient::async_connect_handler(const boost::system::error_code& ec, boost::system::error_code* output_ec)
{
	*output_ec = ec;
}

void lbne::PennClient::async_completion_handler(
		const boost::system::error_code &error_code, std::size_t length,
		boost::system::error_code* output_error_code, std::size_t* output_length)
{
	*output_error_code = error_code;
	*output_length = length;
}

std::size_t lbne::PennClient::send(std::string const & send_str)
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
			boost::bind(&lbne::PennClient::async_completion_handler, _1, _2, &error, &send_len));

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
		DAQLogger::LogError("PennClient") << "Connection closed by PENN";
	}
	else if (error == boost::asio::error::operation_aborted)
	{
		// Timeout signalled by deadline actor
		DAQLogger::LogError("PennClient") << "Timeout sending message to PENN";
	}
	else if (error)
	{
		DAQLogger::LogError("PennClient") << "Error sending message to PENN: " << error.message();
	}
	else if (send_len != send_str.size())
	{
		DAQLogger::LogError("PennClient") << "Size mismatch when sending transaction: wrote " << send_len << " expected " << send_str.size();
	}

	return send_len;
}

// std::string lbne::PennClient::receive(void)
// {

// 	this->set_deadline();

// 	std::size_t receive_length = 0;
// 	boost::system::error_code error = boost::asio::error::would_block;

// 	boost::array<char, 128> raw_buffer;

// 	socket_.async_read_some(boost::asio::buffer(raw_buffer),
// 			boost::bind(&lbne::PennClient::async_completion_handler, _1, _2, &error, &receive_length));

// 	do
// 	{
// 		io_service_.run_one();
// 	}
// 	while (error == boost::asio::error::would_block);

// 	return std::string(raw_buffer.data(), receive_length);
// }

std::string lbne::PennClient::receive(void)
{
  this->set_deadline();
  std::size_t receive_length = 0;
  boost::system::error_code error = boost::asio::error::would_block;

  boost::asio::streambuf response;
  
  boost::asio::async_read_until(socket_, response, "\f",
		boost::bind(&lbne::PennClient::async_completion_handler, _1, _2, &error, &receive_length));

  do
  {
    io_service_.run_one();
  }
  while (error == boost::asio::error::would_block);

  boost::asio::streambuf::const_buffers_type bufs = response.data();
  return std::string(boost::asio::buffers_begin(bufs), boost::asio::buffers_begin(bufs) + response.size());

}

void lbne::PennClient::set_param_(std::string const & name, std::string const & encoded_value, std::string const & type)
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
	  DAQLogger::LogError("PennClient") << "SET command failed: " << response;
	}

}

bool lbne::PennClient::response_is_ack(std::string const & response, std::string const & command)
{
	bool is_ack = true;
	std::vector<std::string> tokens;

	boost::split(tokens, response, boost::is_any_of(" "));
	is_ack = ((tokens[0] == "ACK") && (tokens[1] == command));

	return is_ack;
}
