/*
 * RceClient.hh
 *
 *  Created on: Jun 26, 2014
 *      Author: tcn45
 */

#ifndef RCECLIENT_HH_
#define RCECLIENT_HH_

#include <string>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/lexical_cast.hpp>

using boost::asio::ip::tcp;

namespace lbne {

	class RceClient
	{
	public:
		RceClient(const std::string& host_name, const std::string& port_or_service, const unsigned int timeout_ms);
		virtual ~RceClient();

		void send_command(std::string const & command);
		template<class T> void set_param(std::string const & name, T const & value, std::string const & );

	private:

		std::size_t send(std::string const & send_str);
		std::string receive(void);
		void set_param_(std::string const& name, std::string const & encoded_value, std::string const & type);
		bool response_is_ack(std::string const & response, std::string const & command);

		void set_deadline(void);
		void check_deadline(void);
		static void async_connect_handler(const boost::system::error_code& ec, boost::system::error_code* output_ec);
		static void async_completion_handler(
				const boost::system::error_code &error_code, std::size_t length,
				boost::system::error_code* output_error_code, std::size_t* output_length);

		boost::asio::io_service     io_service_;
		tcp::socket                 socket_;
		boost::asio::deadline_timer deadline_;
		unsigned int                timeout_ms_;

	};

	template<class T>
		void RceClient::set_param(std::string const & name, T const & value, std::string const & type)
	{
		set_param_(name, boost::lexical_cast<std::string>(value), type);
	}

}


#endif /* RCECLIENT_HH_ */
