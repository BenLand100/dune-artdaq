/*
 * L1Receiver.cpp
 *
 *  Created on: Feb 15, 2016
 *      Author: Enrico.Gamberini@cern.ch
 */

#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "RequestReceiver.hh"
#include <cstring>
//using namespace boost::asio;

RequestReceiver::RequestReceiver(std::string & addr) : 
  m_subscribeAddress(addr),
  m_stop_thread{ false }
{
 m_req = std::make_unique<RequestQueue_t>(200);
 }


RequestReceiver::~RequestReceiver() {
  // close socket
  m_req.reset(nullptr);
}

void RequestReceiver::start() {
  // Create the zmq context and socket
  m_ctx = zmq_ctx_new();
  m_socket = zmq_socket(m_ctx, ZMQ_SUB);

  // Connect the socket to the other end, and subscribe to all the messages on it
  int zrc = zmq_connect(m_socket, m_subscribeAddress.c_str());
  if (zrc!=0) {
    dune::DAQLogger::LogWarning("RequestReceiver::start")
      << "ZMQ connect return code is not zero, but: " << zrc;   
  } else {
     dune::DAQLogger::LogInfo("RequestReceiver::start")
      << "Connected to ZMQ socket successfully!";
  }
  zmq_setsockopt(m_socket, ZMQ_SUBSCRIBE, "", 0);

  m_stop_thread = false;
  m_prevTrigger.seqID = 0;
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

  zmq_close(m_socket);
  zmq_ctx_destroy(m_ctx);

}

TriggerInfo RequestReceiver::getNextRequest() {
  TriggerInfo request;
  // Based on queue check; return a request if there is a valid one, else return a dummy request if 2 seconds have elapsed.
  auto startTime=std::chrono::system_clock::now();
  auto now=std::chrono::system_clock::now();
  while ( m_req->isEmpty() && (now - startTime) < std::chrono::seconds(2) ) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    now=std::chrono::system_clock::now();
  } 
  if ( m_req->isEmpty()) {
    request.seqID = 0;
    request.timestamp = 0;
  }
  else {
    m_req->read( std::ref(request));
    if ( request.seqID != m_prevTrigger.seqID+1 )
    {
      dune::DAQLogger::LogWarning("RequestReceiver::getNextRequest") << "Received a sequence id in not the right order! Previous:" << m_prevTrigger.seqID << " new:" << request.seqID;
    }
    m_prevTrigger.seqID = request.seqID;
  }
  return request;
}

// Are there more message parts waiting on the socket?
bool RequestReceiver::rcvMore()
{
  int rcvmore;
  size_t option_len=sizeof(rcvmore);
  zmq_getsockopt(m_socket, ZMQ_RCVMORE, &rcvmore, &option_len);
  return rcvmore;
}

std::vector<uint64_t> RequestReceiver::getVals()
{
  std::vector<uint64_t> ret;
  uint64_t val;
  int rc=zmq_recv(m_socket, &val, sizeof(val), ZMQ_DONTWAIT);

  // Did we get anything on the socket?
  if(rc==-1 && errno==EAGAIN){
    // No, so return empty vector
    return ret;
  }
  else if(rc==-1){
    // Some other error. Print it and return empty vector
    dune::DAQLogger::LogError("RequestReceiver::getVals") << "Error in zmq_recv(): " << strerror(errno);
    return ret;
  }
  else{
    // Yes, so receive the whole message
    ret.push_back(val);
    while(rcvMore()){
      zmq_recv(m_socket, &val, sizeof(val), 0);
      ret.push_back(val);
    }
    return ret;
  }
}


void RequestReceiver::thread(){
  dune::DAQLogger::LogInfo("RequestReceiver::thread") << "Starting listening loop";
  // Spin while stop issued.
  while(!m_stop_thread){
    std::vector<uint64_t> vals=getVals();
    if(vals.empty()){
      // There was no message waiting.
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    else {
      TriggerInfo t;
      t.seqID = vals[0];
      t.timestamp = vals[5];
      m_req->write(t);
    }
  }
}

