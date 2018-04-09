#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "NetioHandler.hh"
#include "NetioWIBRecords.h"
#include "ReusableThread.hh"

#include <ctime>
#include <iomanip>
#include <pthread.h>

using namespace dune;

NetioHandler::NetioHandler()
{
  m_activeChannels=0;

  m_nmessages=0;
  m_msgsize = 472;

  m_stop_subs=false;
  m_cpu_lock=false;

  m_bufferPtr=nullptr;
  m_bytesReadPtr=nullptr;

  m_triggerTimestamp=0;
  m_triggerSequenceId=0;

  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler NetioHandler::NetioHandler")
      << "NIOH setup complete. Background thread spawned.";
  }
}

NetioHandler::~NetioHandler() {
  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler NetioHandler::~NetioHandler")
      << "NIOH terminate ongoing... Stopping communication with FELIX."; 
  }  
  m_stop_trigger=true;
  m_stop_subs=true;

  // RS: Check this! We need a proper FMLINK - CHN - TAG mapping!
  //for ( auto sock : m_sub_sockets ) { sock.second->unsubscribe(0, netio::endpoint(m_felixHost, m_felixRXPort)); delete sock.second; }
  //m_sub_sockets.clear();

  m_pcqs.clear(); 
  if (m_verbose) { DAQLogger::LogInfo("NetioHandler NetioHandler::~NetioHandler")
      << "NIOH terminated. Clean shutdown."; }
}

bool NetioHandler::setupContext(std::string contextStr) {
  DAQLogger::LogInfo("NetioHandler NetioHandler::setupContext")
    << "Creating context and starting bacground thread for " << contextStr; 
  m_context = new netio::context(contextStr);
  m_netio_bg_thread = std::thread( [&](){m_context->event_loop()->run_forever();} );
  set_thread_name(m_netio_bg_thread, "nioh-bg", 0);
  return true;
}

bool NetioHandler::stopContext() {
  DAQLogger::LogInfo("NetioHandler NetioHandler::stopContext")
    << "Stopping eventloop, destroying context."; 
  m_context->event_loop()->stop();
  m_netio_bg_thread.join();
  delete m_context;
  return true;
}

void NetioHandler::startTriggerMatchers(){
  m_stop_trigger.store(false);
  for (uint32_t i=0; i<m_activeChannels; ++i){
    m_extractors.emplace_back( new ReusableThread(i) );
    set_thread_name(m_extractors[i]->get_thread(), "nioh-trm", i);
    uint32_t tid = i;
    m_functors.push_back( [&, tid] { 
      // segfaults on accessing any member of NIOH if stopDatataking() is issued... (not, when timing is on!)
      if (m_stop_trigger.load()) { std::cout<<" Bail out... \n"; return; }

      // Trigger matching:
      if (m_extract) {

        WIBHeader wh = *(reinterpret_cast<const WIBHeader*>( m_pcqs[tid]->frontPtr()+sizeof(FromFELIXHeader) ));
        uint_fast64_t frontTS  = wh.timestamp();
        uint_fast64_t timeDiff = m_triggerTimestamp-frontTS;
        uint_fast64_t posDiff  = timeDiff/(uint_fast64_t)m_tickdist;
        size_t qsize = m_pcqs[tid]->sizeGuess();
        int32_t leftBound = qsize - m_timeWindow;
        uint32_t rightBound = qsize + m_timeWindow;
        if ( leftBound < 0 || rightBound > m_pcqs[tid]->capacity() ) {
          DAQLogger::LogInfo("NetioHandler NetioHandler::startTriggerMatchers")
            << "WARNING! You are close to queue boundaries! left(qsize-timeWindow):"
            << leftBound << " right(qsize+timeWindow):" << rightBound << "  For safety, I'll skip it now... \n";
          return;
        }
        
        //m_pcqs[tid]->popXFront(posDiff); // pop with caution.

        // Single copy of ~4.6 MByte : 
        //memcpy out from:(const char*)m_pcqs[tid]->frontPtr()  bytes:m_msgsize*m_timeWindow
        memcpy(m_fragmentPtr->dataBeginBytes(), (char*)m_pcqs[tid]->frontPtr(), 512*m_timeWindow);
    
        uint64_t newTS = *(reinterpret_cast<const uint_fast64_t*>(m_fragmentPtr->dataBeginBytes()+8));

        DAQLogger::LogInfo("NetioHandler NetioHandler::startTriggerMatchers")
          << "Trigger match for timestamp:" << std::hex << m_triggerTimestamp << std::dec << '\n'
          << "Queue to search on:" << &m_pcqs[tid] << " fragment to fill:" << &m_fragmentPtr << '\n' << std::hex
          << "Timestamp on front:" << wh.timestamp() << " difference to request:" << timeDiff << '\n'
          << "Position to jump:" << std::dec << posDiff << " (queueSize approx: " << qsize << ")" << '\n'
          << "Copied out 512*10000 bytes.\n"
          << "Worked??" << std::hex << newTS << std::dec;
         

      } else {
        std::cout << " EXTRACTOR WOOF -> Dry run, won't extract... simulate some work.. (sleep for 100ms)\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << " I'M DONE!!!!\n";
      }
    });
  }
}

void NetioHandler::stopTriggerMatchers(){
  DAQLogger::LogInfo("NetioHandler NetioHandler::stopTriggerMatchers")
    << "Attempt to stop triggerMatchers... Wait until they are busy!";
  m_stop_trigger.store(true);
  bool success = false;
  while (!success){
    for (uint32_t i=0; i<m_extractors.size(); ++i){
      if ( m_extractors[i]->get_readiness() == true ) {
        success |= true;
      }  else {
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        success |= false;
      }
      //std::cout << "  -> " << success << '\n';
    }
  }
  m_extractors.clear();
}

bool NetioHandler::busy(){
  uint32_t busyLinks = 0;
  for (uint32_t i=0; i<m_extractors.size(); ++i) {
    bool busy = (m_extractors[i]->get_readiness()==false) ? true : false; 
    if (busy) { busyLinks++; }
  }
  return (busyLinks != 0) ? true : false;
}

bool NetioHandler::triggerWorkers(uint64_t timestamp, uint64_t sequence_id, std::unique_ptr<artdaq::Fragment>& frag) {
  if (m_stop_trigger.load()) return false; // check if we should proceed with the trigger.
 
  m_triggerTimestamp = timestamp;
  m_triggerSequenceId = sequence_id;
  m_fragmentPtr = frag.get();
  DAQLogger::LogInfo("NetioHandler NetioHandler::triggerWorkers")
    << "Trigger workers for timestamp:" << std::hex << m_triggerTimestamp 
    << " seqId:" << m_triggerSequenceId << std::dec;

  // Set functors for extractors.
  for (uint32_t i=0; i<m_activeChannels; ++i){
    m_extractors[i]->set_work(m_functors[i]);
  } 
  while (busy()){
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  return true;
/*
  while ( m_extractors[0]->get_readiness() == false ) { // while we wait, do some other stuff if needed...
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  if ( m_extractors[0]->get_readiness() == true ) {
    return true;
  } else {
    return false;
  }
*/
}

void NetioHandler::startSubscribers(){
  for (uint32_t i=0; i<m_activeChannels; ++i){
    uint32_t chn = i; 
    m_netioSubscribers.push_back(
      std::thread([&, chn](){
        DAQLogger::LogInfo("NetioHandler NetioHandler::startSubscribers") << "Subscriber spawned for link " << chn;
        netio::endpoint ep;
        netio::message msg;
        size_t goodOnes=0;
        size_t badOnes=0;
        std::vector<size_t> badMessages;
        std::vector<std::pair<uint_fast64_t, uint_fast64_t>> distFails;
        while(!m_stop_subs){
          m_sub_sockets[m_channels[chn]]->recv(ep, std::ref(msg));
          m_nmessages++;
          if ((msg.size()!=m_msgsize)){ 
            badOnes++;
            badMessages.push_back( msg.size() );
            continue; 
          }

          // RS -> Add possibility for dry_run! (No push mode.)        
          WIB_CHAR_STRUCT wcs;
          msg.serialize_to_usr_buffer((void*)&wcs);
          m_pcqs[m_channels[chn]]->write(std::move(wcs));
          goodOnes++;

          // RS -> Add more sophisticated quality check.          

          continue;
        }
        DAQLogger::LogInfo("NetioHandler NetioHandler::subscriber") 
          << "Subscriber joining for link " << chn << '\n' 
          << "Failure summary: sum(BAD)" << badOnes << " sum(GOOD)" << goodOnes;
        //for (unsigned i=0; i<badMessages.size(); ++i){
        //  std::cout << "BAD MSG SIZE: " << badMessages[i] << '\n';
        //}
      })
    );
    set_thread_name(m_netioSubscribers[i], "nioh-sub", i);
  }
}

void NetioHandler::stopSubscribers(){
  m_stop_subs=true;
  for (uint32_t i=0; i<m_netioSubscribers.size(); ++i) {
    m_netioSubscribers[i].join();
  }
  m_netioSubscribers.clear();
}

void NetioHandler::lockSubsToCPUs() {
  DAQLogger::LogInfo("NetioHandler NetioHandler::lockSubsToCPUs") 
          << "Attempt to lock to CPUs. Hardcoded for NUMA0!"; 
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  unsigned short cpuid = 0;
  for (unsigned i=0; i< m_netioSubscribers.size(); ++i) {
    CPU_SET(cpuid, &cpuset);
    int ret = pthread_setaffinity_np(m_netioSubscribers[i].native_handle(), sizeof(cpu_set_t), &cpuset);
    if (ret!=0) {
      DAQLogger::LogError("NetioHandler NetioHandler::lockSubsToCPUs") 
        << "Error calling pthread_setaffinity! Return code:" << ret; 
    } else {
      DAQLogger::LogInfo("NetioHandler NetioHandler::lockSubsToCPUs") 
        << "Subscriber locked to CPU["<<cpuid<<"]"; 
    }
//#warning RS: FIXME! Subscriber NUMA specific CPU locks should come from FHiCL configuration!
    cpuid+=2; // RS: FIXME! This should come from a NUMA node mapping from configuration, or through auto-discovery.
  }
  m_cpu_lock = true;
}

bool NetioHandler::addChannel(uint64_t chn, uint16_t tag, std::string host, uint16_t port, size_t queueSize){
  m_channels.push_back(chn);
  m_pcqs[chn] = std::make_unique<FrameQueue>(queueSize);
  //std::cout << " QUEUE AT: " << &m_pcqs[chn] << '\n';
  try {
    // RS -> ADD ZERO COPY SUPPORT.
    //netio::sockcfg cfg = netio::sockcfg::cfg();
    //if (zero_copy) { cfg(netio::sockcfg::ZERO_COPY); }

    netio::sockcfg cfg = netio::sockcfg::cfg();
    //m_send_sockets[chn] = new netio::low_latency_send_socket(m_context);    
    m_sub_sockets[chn] = new netio::subscribe_socket(m_context, cfg);
    netio::tag elink = chn*2;
    netio::tag checkTag = tag;
    if ( elink != checkTag ) { std::cout << "Something is fishy here...\n"; }
    m_sub_sockets[chn]->subscribe(elink, netio::endpoint(host, port));
    std::this_thread::sleep_for(std::chrono::microseconds(10000)); // This is needed... :/
  } catch(...) {
    std::cout << "### NetioHandler::addChannel(" << chn << ") -> ERROR. Failed to activate channel.\n";
    DAQLogger::LogError("NetioHandler NetioHandler::addChannel") 
      << "Failed to add channel! chn:" << chn << " tag:" << tag << " host:" << host << " port:" << port;
    return false;
  }
  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler NetioHandler::addChannel") 
      << "Activated channel in NIOH. chn:" << chn << " tag:" << tag << " host:" << host << " port:" << port;
  }
  m_activeChannels++;
  return true;
} 


