#ifndef lbne_artdaq_RCConnection_RCConnection_hh
#define lbne_artdaq_RCConnection_RCConnection_hh

// JCF, 4/26/15

// RCConnection is a singleton class (i.e., it's only possible to
// create one instance of it in a process) which contains the ZeroMQ
// connection to lbne35t RunControl, and through which all messages to
// RunControl in lbne-artdaq must be sent.

// Since its constructor is private, access is done by the following:

// RCConnection& myconnection = RCConnection::Get();

#include "zmq.hpp"

#include <string>
#include <memory>

namespace lbne {

class RCConnection {

public:

  static RCConnection& Get() {
    static RCConnection connection;
    return connection;
  }

  void Send(const std::string msg);

private:
  RCConnection();  // Guarantee the class completely controls its own
		   // creation; necessary for a singleton

  void InitConnection();

  const std::string runcontrol_socket_address_;
  bool connection_opened_;
  std::unique_ptr<zmq::context_t> context_;
  std::unique_ptr<zmq::socket_t> socket_;
};

}

#endif // lbne_artdaq_RCConnection_RCConnection_hh
