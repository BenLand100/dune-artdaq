#ifndef dune_artdaq_RCConnection_RCConnection_hh
#define dune_artdaq_RCConnection_RCConnection_hh

// JCF, 4/26/15

// RCConnection is a singleton class (i.e., it's only possible to
// create one instance of it in a process) which contains the ZeroMQ
// connection to dune35t RunControl, and through which all messages to
// RunControl in an dune-artdaq process must be sent.

// Since its constructor is private, access is done by calling the
// RCConnection::Get() function

#include "zmq.hpp"
#include "I3JSON.hh"

#include <string>
#include <memory>
#include <mutex>

namespace dune {

class RCConnection {

public:

  static RCConnection& Get() {
    static RCConnection connection;
    return connection;
  }

  void SendMessage(const std::string& service, const std::string& msg, 
		   const std::string& severity);

  template <typename T>
  void SendMetric(const std::string& service, const std::string& varname, 
		  const T& value) {

    std::string varname_nospaces = varname;
    std::replace( varname_nospaces.begin(), varname_nospaces.end(), ' ', '.');
    std::string json_msg = MetricToRCJSON<T>(service, varname_nospaces, value);
    Send(json_msg);
  }


private:
  RCConnection();  // Guarantee the class completely controls its own
		   // creation; necessary for a singleton

  void InitConnection();

  void Send(const std::string& json_msg);

  const std::string runcontrol_socket_address_;
  bool connection_opened_;
  std::unique_ptr<zmq::context_t> context_;
  std::unique_ptr<zmq::socket_t> socket_;
  std::mutex mutex_;

};

}

#endif // dune_artdaq_RCConnection_RCConnection_hh
