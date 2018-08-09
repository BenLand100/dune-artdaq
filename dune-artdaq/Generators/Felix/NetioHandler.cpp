#include "dune-artdaq/DAQLogger/DAQLogger.hh"
#include "NetioHandler.hh"
#include "NetioWIBRecords.hh"
#include "ReusableThread.hh"

#include <ctime>
#include <iomanip>
#include <pthread.h>

using namespace dune;

NetioHandler::NetioHandler()
{
  m_activeChannels=0;

  m_nmessages=0;
  m_msgsize = 2784;
  m_stop_subs=false;
  m_cpu_lock=false;
  m_extract = true;
  //m_bufferPtr=nullptr;
  //m_bytesReadPtr=nullptr;

  m_triggerTimestamp=0;
  m_triggerSequenceId=0;

  m_turnaround=true;
  m_lastPosition = nullptr;
  m_lastTimestamp = 0x0;

  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler::NetioHandler")
      << "NIOH setup complete. Background thread spawned.";
  }
}

NetioHandler::~NetioHandler() {
  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler::~NetioHandler")
      << "NIOH terminate ongoing... Stopping communication with FELIX."; 
  }  
  m_stop_trigger=true;
  m_stop_subs=true;

  // RS: Check this! We need a proper FMLINK - CHN - TAG mapping!
  //for ( auto sock : m_sub_sockets ) { sock.second->unsubscribe(0, netio::endpoint(m_felixHost, m_felixRXPort)); delete sock.second; }
  //m_sub_sockets.clear();

  m_pcqs.clear(); 
  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler::~NetioHandler")
      << "NIOH terminated. Clean shutdown."; 
  }
}

bool NetioHandler::setupContext(std::string contextStr) {
  DAQLogger::LogInfo("NetioHandler::setupContext")
    << "Creating context and starting bacground thread for " << contextStr; 
  m_context = new netio::context(contextStr);
  m_netio_bg_thread = std::thread( [&](){m_context->event_loop()->run_forever();} );
  set_thread_name(m_netio_bg_thread, "nioh-bg", 0);
  return true;
}

bool NetioHandler::stopContext() {
  DAQLogger::LogInfo("NetioHandler::stopContext")
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
    uint32_t framesPerMsg = m_msgsize/m_framesize;
    size_t frameCapacity = m_pcqs[tid]->capacity() * framesPerMsg;

    //bool reset = false;
    //uint_fast64_t = last_ts = 0x0;
    m_functors.push_back( [&, tid, frameCapacity, framesPerMsg] {
      // segfaults on accessing any member of NIOH if stopDatataking() is issued... (not, when timing is on!)
      // RS -> What is this? It was definitely a hack back at the time. To be checked if can be avoided. 
      //    -> In a clean flow, this should never happen. (maybe good to have it for a safety measure.)
      if (m_stop_trigger.load()) {
        DAQLogger::LogInfo("NetioHandler::startTriggerMatchers") 
          << "Should stop triggers, bailing out.\n";
        return;
      }
      
      // Trigger matching: 
      // RS-> m_extract will go away.
      //      Doesn't make sense to call triggerMatchers if you don't want to fill the buffer.
      //      Or at least write empty data?
      if (m_extract) {

        // Queue is empty?
        // RS -> Keeping this for additional safety measures. 
        //       To be removed when RC ensures that core is running.
        if (m_pcqs[tid]->isEmpty()) {
          DAQLogger::LogWarning("NetioHandler::startTriggerMatchers") 
            << "Queue is empty... Is FelixCore publishing data?\n";
          return;
        }

	// GLM: if Timestamp is 0, check if the queue is filling up and throw away oldest data
	if (m_triggerTimestamp == 0) {
          /*
          size_t queuedElements = m_pcqs[tid]->sizeGuess() - 2; 
          DAQLogger::LogInfo("NetioHandler::startTriggerMatchers") << "Remove " << queuedElements << " old fragments.";
          while(queuedElements) {
            --queuedElements;
	  */

          //if (m_pcqs[tid]->sizeGuess() > 0.5 * m_pcqs[tid]->capacity()) {
	     DAQLogger::LogInfo("NetioHandler::startTriggerMatchers") << "Removing old non requested data; (" << m_pcqs[tid]->sizeGuess()<< ") messages in queue.";
	     m_pcqs[tid]->popXFront(m_pcqs[tid]->sizeGuess());
	    //m_pcqs[tid]->popXFront(m_pcqs[tid]->capacity()/2);
	  //}
	  return;
	}

        // GLM: Try this simplistic approach: do trigger matching and empty the queue until that point
        //      Requires time ordered trigger requests!!


        uint_fast64_t startWindowTimestamp = m_triggerTimestamp - (uint_fast64_t)(m_windowOffset * m_tickdist);
        WIBHeader wh = *(reinterpret_cast<const WIBHeader*>( m_pcqs[tid]->frontPtr() ));
        m_lastTimestamp = wh.timestamp();
        m_positionDepth = 0;
	//GLM: Check if there is such a delay in trigger requests that data have gone already...
	if (m_lastTimestamp > startWindowTimestamp ) {
	  DAQLogger::LogWarning("NetioHandler::startTriggerMatchers") 
	    << "Requested data are so old that they were dropped. Trigger request TS = " 
	    << m_triggerTimestamp << ", oldest TS in queue = "  << m_lastTimestamp;
	  return;
	}

        uint_fast64_t timeTickDiff = (startWindowTimestamp-m_lastTimestamp)/(uint_fast64_t)m_tickdist;
        // wait to have enough stuff in the queue
	uint_fast64_t minQueueSize = (timeTickDiff + m_timeWindow)/framesPerMsg + 10 ; // make sure we don't overtake the write ptr
        size_t qsize = m_pcqs[tid]->sizeGuess();
	uint_fast32_t waitingForDataCtr = 0;	
        while (qsize < minQueueSize) {
	  std::this_thread::sleep_for(std::chrono::microseconds(100));
	  qsize = m_pcqs[tid]->sizeGuess(); 
	  ++waitingForDataCtr;
	  if (waitingForDataCtr > 20000) {
	    DAQLogger::LogWarning("NetioHandler::startTriggerMatchers")
	      << "Data stream delayed by over 2 secs with respect to trigger requests! ";
	    return;
	  }
        }
        // read everything that is older than the TS
	//DAQLogger::LogInfo("NetioHandler::startTriggerMatchers")
	//  << "Jumping by " << timeTickDiff/framesPerMsg << " elements in data queue. Queue size is " << m_pcqs[tid]->sizeGuess();
	m_pcqs[tid]->popXFront(timeTickDiff/framesPerMsg);

	m_fragmentPtr->resizeBytes(m_msgsize*(2 + m_timeWindow/framesPerMsg));
        for(unsigned i=0; i<(m_timeWindow/framesPerMsg)+2; i++) //read out 21 messages
        {
          memcpy(m_fragmentPtr->dataBeginBytes()+m_msgsize*i,(char*)m_pcqs[tid]->frontPtr(), m_msgsize);
          m_pcqs[tid]->popFront();
        }        
      } else { // generate fake fragment for emulation
        if (m_triggerTimestamp == 0) {
          DAQLogger::LogInfo("NetioHandler::startTriggerMatchers") << "no trigger ";
          return;
        } else {
          m_fragmentPtr->resizeBytes(m_msgsize*(2 + m_timeWindow/framesPerMsg));
          char *buf = new char[m_msgsize*(2 + m_timeWindow/framesPerMsg)];
          memcpy(m_fragmentPtr->dataBeginBytes(), buf, 2 + m_timeWindow/framesPerMsg);
        }
      }
    });
  }
}

void NetioHandler::stopTriggerMatchers(){
  DAQLogger::LogInfo("NetioHandler::stopTriggerMatchers")
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

  // Set functors for extractors.
  for (uint32_t i=0; i<m_activeChannels; ++i){
    m_extractors[i]->set_work(m_functors[i]);
  } 
  while (busy()){
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
  if (m_triggerTimestamp == 0) {
    return false;
  }
  return true;
}

void NetioHandler::startSubscribers(){
  if (!m_extract) return;

  for (uint32_t i=0; i<m_activeChannels; ++i) {
    uint32_t chn = i; 
    m_netioSubscribers.push_back(

      std::thread([&, chn](){
        DAQLogger::LogInfo("NetioHandler::startSubscribers") << "Subscriber spawned for link " << chn;
        netio::endpoint ep;
        netio::message msg;
        size_t goodOnes=0;
        size_t badOnes=0;
        size_t lostData=0;

        std::vector<size_t> badSizes;
        std::vector<size_t> badFrags;
	uint64_t expDist = (m_msgsize/m_framesize)*m_tickdist;
        std::vector<std::pair<uint_fast64_t, uint_fast64_t>> distFails;
        while (!m_stop_subs) {
          m_sub_sockets[m_channels[chn]]->recv(ep, std::ref(msg));
          m_nmessages++;
          if (msg.size()!=m_msgsize) { // non-serializable
            badOnes++;
            badSizes.push_back(msg.size());
            badFrags.push_back(msg.num_fragments());
            //DAQLogger::LogWarning("NetioHandler::subscriber")
            //  << " Received message with non-expected size! Bad omen!"
            //  << " -> Trigger matching is unreliable until next queue turnaround!\n";
          }     
	  else {
	    SUPERCHUNK_CHAR_STRUCT ics;
	    msg.serialize_to_usr_buffer((void*)&ics);
	    bool storeOk = m_pcqs[m_channels[chn]]->write( std::move(ics) ); // RS -> Add possibility for dry_run! (No push mode.)
	    if (!storeOk) {
	      if(lostData%10000 == 0) {
		DAQLogger::LogWarning("NetioHandler::subscriber") << " Fragments queue is full. Lost: " << lostData;
	      }
	      ++lostData;
	    }
	    goodOnes++;
          }
          // RS -> Add more sophisticated quality check.
          /*newTs = *(reinterpret_cast<const uint_fast64_t*>((&wcs)+8)); 
          if ( (newTs-ts)!= expDist ){
            distFails.push_back( std::make_pair(ts, newTs) );
          }
          ts = newTs;*/
        }

        // unsubscriber from publisher
        //m_sub_sockets[chn]->unsubscribe(chn, netio::endpoint(m_host, m_port));
        //DAQLogger::LogInfo("NetioHandler::subscriber") << "Joining... Unsubscribed from channel[" << chn << "]";
        
        // Joining subscriber...
        std::ostringstream subsummary;
        for (unsigned i=0; i<badSizes.size(); ++i){
          subsummary << "(MSG SIZE: " << badSizes[i] << " FRAGS:" << badFrags[i] << "),\n";
        }
        /*std::ostringstream distsummary;
        for (unsigned i=0; i< distFails.size(); ++i){
          distsummary << "BAD DISTANCE: ts0:" << std::hex << distFails[i].first << " t1:" << distFails[i].second
                      << " dist:" << std::dec << distFails[i].second - distFails[i].first << '\n';
        }*/
        DAQLogger::LogInfo("NetioHandler::subscriber") 
          << " -> Subscriber joining for link " << chn << '\n' 
          << " -> Failure summary: sum(BAD) " << badOnes << " sum(GOOD) " << goodOnes << " sum(LOST) " << lostData << '\n'
          << subsummary.str()
          << " -> Failed timestamp distances (expected distance between messages: " << expDist << ")\n";
          //<< distsummary.str();
	})
				 );
    set_thread_name(m_netioSubscribers[i], "nioh-sub", i);
  }
}

void NetioHandler::stopSubscribers(){
  m_stop_subs=true;
  for (uint32_t i=0; i<m_netioSubscribers.size(); ++i) {
    m_netioSubscribers[i].join();
    DAQLogger::LogInfo("NetioHandler::stopSubscriber") << "Subscriber[" << i << "] joined."; 
  }
  m_netioSubscribers.clear();
}

void NetioHandler::lockSubsToCPUs(uint32_t offset) {
  DAQLogger::LogInfo("NetioHandler::lockSubsToCPUs") << "Attempt to lock to CPUs. Hardcoded for NUMA0!"; 
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  unsigned short cpuid = offset;
  for (unsigned i=0; i< m_netioSubscribers.size(); ++i) {
    CPU_SET(cpuid, &cpuset);
    // RS : Not a good attempt.
    //CPU_SET(cpuid+16, &cpuset);
    int ret = pthread_setaffinity_np(m_netioSubscribers[i].native_handle(), sizeof(cpu_set_t), &cpuset);
    if (ret!=0) {
      DAQLogger::LogError("NetioHandler::lockSubsToCPUs") 
        << "Error calling pthread_setaffinity! Return code:" << ret; 
    } else {
      DAQLogger::LogInfo("NetioHandler::lockSubsToCPUs") 
        << "Subscriber locked to CPU["<<cpuid<<"]"; 
    }
//#warning RS: FIXME! Subscriber NUMA specific CPU locks should come from FHiCL configuration!
    cpuid+=2; // RS: FIXME! This should come from a NUMA node mapping from configuration, or through auto-discovery.
  }
  m_cpu_lock = true;
}

bool NetioHandler::addChannel(uint64_t chn, uint16_t tag, std::string host, uint16_t port, size_t queueSize, bool zerocopy){
  m_host=host;
  m_port=port;
  m_channels.push_back(chn);
  m_pcqs[chn] = std::make_unique<FrameQueue>(queueSize);
  if (m_extract) {
  try {
    netio::sockcfg cfg = netio::sockcfg::cfg(); 
    if (zerocopy) cfg(netio::sockcfg::ZERO_COPY);
    m_sub_sockets[chn] = new netio::subscribe_socket(m_context, cfg);
    netio::tag elink = tag;
    m_sub_sockets[chn]->subscribe(elink, netio::endpoint(host, port));
    std::this_thread::sleep_for(std::chrono::microseconds(10000)); // This is needed... Why? :/
    DAQLogger::LogInfo("NetioHandler::addChannel") 
      << "Added channel chn:" << chn << " tag:" << tag << " host:" << host << " port:" << port;
  } catch(...) {
    DAQLogger::LogError("NetioHandler::addChannel") 
      << "Failed to add channel! chn:" << chn << " tag:" << tag << " host:" << host << " port:" << port;
    return false;
  }
  if (m_verbose) { 
    DAQLogger::LogInfo("NetioHandler::addChannel") 
      << "Activated channel in NIOH. chn:" << chn << " tag:" << tag << " host:" << host << " port:" << port;
  }
  }
  m_activeChannels++;
  return true;
} 


