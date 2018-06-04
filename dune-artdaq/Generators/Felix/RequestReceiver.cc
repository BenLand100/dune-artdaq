/*
 * L1Receiver.cpp
 *
 *  Created on: Feb 15, 2016
 *      Author: Enrico.Gamberini@cern.ch
 */

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "RequestReceiver.hh"

using namespace boost::asio;

RequestReceiver::RequestReceiver() : 
  m_socket(m_io_service),
  m_listenAddress(""),
  m_multicastAddress(""),
  m_multicastPort(0),
  m_max_requests(0),
  m_stop_thread{ false }
{
}

RequestReceiver::~RequestReceiver() {
  // close socket
  m_req.reset(nullptr);
  m_socket.close();
}

bool RequestReceiver::setup(const std::string& listenAddress,
                            const std::string& multicastAddress,
                            const unsigned short& multicastPort,
                            const unsigned short& requestSize ) {
  m_listenAddress = listenAddress;
  m_multicastAddress = multicastAddress;
  m_multicastPort = multicastPort;
  m_max_requests = requestSize;
  m_req = std::make_unique<RequestQueue_t>(m_max_requests);

  dune::DAQLogger::LogInfo("RequestReceiver::setup")
      << "Attempt to setup. ListenAddress:" << listenAddress 
      << " MulticastAddress:" << multicastAddress << " MulticastPort:" << multicastPort;

  // Create the socket
  try {
    ip::udp::endpoint listen_endpoint( ip::udp::v4(), m_multicastPort );
    m_socket.open(listen_endpoint.protocol());
    // TODO not sure this is necessary
    m_socket.set_option(ip::udp::socket::reuse_address(true));
    m_socket.bind(listen_endpoint);

    // Join the multicast group
    m_socket.set_option( ip::multicast::join_group(
      ip::address::from_string(m_multicastAddress).to_v4(),
      ip::address::from_string(m_listenAddress).to_v4())
    );
    dune::DAQLogger::LogInfo("RequestReceiver::setup")
      << "Successfully joined to multicast group!";
    return true;
  } catch (...) {
    dune::DAQLogger::LogError("RequestReceiver::setup")
      << "Failed joined to multicast group!";
    return false;
  }
}

void RequestReceiver::start() {
  m_stop_thread = false;
  m_receiver = std::thread(&RequestReceiver::thread, this);
  set_thread_name(m_receiver, "req-recv", 1);
  dune::DAQLogger::LogInfo("RequestReceiver::start")
      << "Request receiver thread started!";
}

void RequestReceiver::stop() {
  m_stop_thread = true;
  if ( m_receiver.joinable() ){
    m_receiver.join();
  } else {
    std::cout << " PANIC! Receiver thread is not joinable!\n";
  }
  dune::DAQLogger::LogInfo("RequestReceiver::stop")
      << "Request receiver thread joined! Request queue flushed.";
}

bool RequestReceiver::requestAvailable() { 
  return !m_req->isEmpty();
}

bool RequestReceiver::getNextRequest(artdaq::detail::RequestPacket& request) {
  // Based on queue check, return true or false, and set request.
  if ( m_req->isEmpty() ) {
    return false; 
  } else {
    m_req->read( std::ref(request) );
    return true;
  }
}

void RequestReceiver::thread(){
  // Thread locals.
  unsigned bytes=0;
  char* dataBuff= new char[1512];
  bool isHeader=true;
  unsigned packets=0;
  uint64_t lastSequence=0;

  // Spin while stop issued.
  while(!m_stop_thread){

    int received = m_socket.receive_from(boost::asio::buffer(dataBuff, 1500), m_remote_endpoint);
    bytes += received;

    if( isHeader ){ // header

      artdaq::detail::RequestHeader* reqMess = reinterpret_cast<artdaq::detail::RequestHeader*>(dataBuff);
      if(reqMess->isValid()){
        packets = reqMess->packet_count;
        isHeader=false;
      }
    } else { // not header
      for( unsigned i=0; i<packets; i++ ) {
        artdaq::detail::RequestPacket* reqPack = reinterpret_cast<artdaq::detail::RequestPacket*>(
          dataBuff+i*sizeof(artdaq::detail::RequestPacket));
        if( reqPack->isValid() ) {
          if( lastSequence < reqPack->sequence_id ) { 
            lastSequence = reqPack->sequence_id;
            m_req->write( artdaq::detail::RequestPacket(*reqPack) );
          }
        }
      }
      isHeader=true;
    }

  }

  // RS -> FREE UP STUFF!!!!
  delete dataBuff;
}

