#include "packetformat/block_format.hpp"
#include "packetformat/block_parser.hpp"
#include "flxcard/FlxCard.h"
#include "flxcard/FlxException.h"
#include "cmem.hpp"
#include "circular_dma.hpp"

#include "dune-artdaq/Generators/Felix/CPUPin.hpp"

#include "dune-artdaq/Generators/Felix/QueueHandler.hh"
#include "dune-artdaq/Generators/Felix/NetioHandler.hh"

#include "ProducerConsumerQueue.hh"

#include "netio/netio.hpp"

#include <iomanip>

//#include <boost/lockfree/spsc_queue.hpp>

//#define TAGPUBLISHER
//#define IOVEC 1024 // <- MAX 510! // IOVEC NEEDS TAGPUBLISHER!
//#define DUMP_SUPCHUNKS // instead of netio messages, create superchunks directly.

// Size of the circular buffer
#define PROTODUNE_MEMSIZE  (8192*1024*1024UL) // (8192*1024*1024UL)

// Number of blocks that is not freed as it might contain data that still belongs
// to a current chunk.
#define MARGIN_BLOCKS (4)

#define SUPERCHUNK_SIZE (5568) // for 12: 5568  for 6: 2784  for 24: 11136

// Threshold. There needs to be this amount of blocks available for the readout loop to start.
#define BLOCK_THRESHOLD (256) //256

#define IOVEC_QUEUE 1

// If set, we copy out blocks for debugging purposes.
// Comment it out for actual runs!
//#define BLOCKDEBUG

const size_t BLOCKSIZE = felix::packetformat::BLOCKSIZE;

class ProtoDuneParser;

std::mutex cout_mutex;


std::string hexStr(const char *data, int len)
{
  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < len; ++i)
    ss << std::setw(2) << std::setfill('0') << (int)data[i];
  return ss.str();
}

inline std::ostream& operator<< (std::ostream &out, const felix::packetformat::block &block)
{
  std::string blockstr = hexStr(&block.data[0], 1020);
  //std::string blockstr(block.data, 1020);
  out << std::hex << blockstr << std::dec << '\n';
  return out;
}

inline std::string blckheader_to_hex(const felix::packetformat::block &block)
{
  std::stringstream ss;
  ss << std::hex
     << " elink:" << (unsigned)block.elink
     << " seqnr:" << (unsigned)block.seqnr
     << " sob:" << (unsigned)block.sob
     << " gbt:" << (unsigned)block.gbt
     << " egroup:" << (unsigned)block.egroup
     << " epath:" << (unsigned)block.epath
     << std::dec;
  return ss.str();
}

inline std::string subchunktrailer_to_hex(const felix::packetformat::subchunk& subchunk)
{
/*
 57   enum Type
  58   {
     59     NULL_=0, FIRST, LAST, BOTH, MIDDLE, TIMEOUT, OUTOFBAND=7
      60   };
       61 
        62   Type           type;
         63   bool           trunc;
          64   bool           chunk_error;
           65   unsigned       length;
            66   const block*   containing_block;
             67   const char*    data;
              68   bool           error = false;
*/

  std::stringstream ss;
  ss << std::hex
     << " type:" << (unsigned)subchunk.type
     << " trunc:" << (unsigned)subchunk.trunc
     << " chunk_error:" << (unsigned)subchunk.chunk_error
     << " error:" << (unsigned)subchunk.error
     << std::dec;
  return ss.str();
}


struct ProtoDuneParserOps : public felix::packetformat::ParserOperations
{
  std::atomic<unsigned> m_packet_ctr;
  std::atomic<int> m_short_ctr;
  std::atomic<int> m_chunk_ctr;
  std::atomic<int> m_subchunk_ctr;
  std::atomic<int> m_block_ctr;
  std::atomic<int> m_error_short_ctr;
  std::atomic<int> m_error_chunk_ctr;
  std::atomic<int> m_error_subchunk_ctr;
  std::atomic<int> m_error_block_ctr;

  bool error = false;
  uint32_t numErrors = 0;
  uint32_t numGood = 0;
  std::atomic<unsigned> m_errorsToReport{0};

  felix::packetformat::block m_prev_block;
  felix::packetformat::block m_err_block;


  unsigned m_id;
  unsigned m_packet_target = 1;
  unsigned m_linkId;
  unsigned m_queueId;

  // QUEUES
  folly::ProducerConsumerQueue<uint32_t> m_error_chunk_sizes;
  folly::ProducerConsumerQueue<felix::packetformat::block> m_q_prev_blocks;
  folly::ProducerConsumerQueue<felix::packetformat::block> m_q_error_blocks;

  //folly::ProducerConsumerQueue<felix::packetformat::block> m_q_blocks;

  std::unique_ptr<folly::ProducerConsumerQueue<SUPERCHUNK_CHAR_STRUCT>>& m_pcq;

#ifdef TAGPUBLISHER
  std::unique_ptr<netio::tag_publisher> m_tagPub;
#endif
#ifdef IOVEC
  iovec* m_vec[IOVEC_QUEUE];
  size_t m_vec_pos;
  size_t m_vec_it;
  size_t m_num_chunks;
  size_t m_total_size;
  //folly::ProducerConsumerQueue<IovToSend> m_iovQueue;

  void flush_iovec()
  {
    m_tagPub->publish(m_vec[m_vec_it],m_vec_pos,m_total_size);
    m_vec_pos=1;
    ++m_vec_it;
    if (m_vec_it == IOVEC_QUEUE) m_vec_it = 0;
    m_num_chunks=0;
    m_total_size=0;
  }
#endif


  ProtoDuneParserOps(unsigned id, unsigned linkId, unsigned queueId) : m_id(id), m_linkId(linkId), m_queueId(queueId),
    m_error_chunk_sizes(1000000),  m_q_prev_blocks(10000), m_q_error_blocks(10000),
    m_pcq{ QueueHandler::getInstance().getQueue(queueId) }
  {

    //uint64_t queueId = m_linkId;
    //if (m_linkId > 1000) {
    //  queueId -= 5;
    //}
    //m_pcq{ QueueHandler::getInstance().getQueue(queueId) };

    //m_pcq = std::make_unique<folly::ProducerConsumerQueue<SUPERCHUNK_CHAR_STRUCT>>(10000);
    //m_pcq NetioHandler::getInstance().getQueue(m_linkId);
    std::cout << "ProtoDuneParserOps[" << m_id << "," << m_linkId << "] Queue reference acquired." << '\n';

#if defined(TAGPUBLISHER) && defined(IOVEC)
    std::cout << "ProtoDuneParserOps[" << m_id << "," << m_linkId << "] Both TAGPUBLISHER and IOVEC is defined. IOVEC set to:" << IOVEC << '\n';
    m_tagPub = m_pub->get_tag_publisher(m_id);
#elif defined(TAGPUBLISHER)
    std::cout << "ProtoDuneParserOps[" << m_id << "," << m_linkId << "]  TAGPUBLISHER defined" << '\n';
    m_tagPub = m_pub->get_tag_publisher(m_id);
#else
    std::cout << "ProtoDuneParserOps[" << m_id << "," << m_linkId << "] No definitions." << '\n';
#endif

    m_packet_ctr=0;
    std::thread statistics([this, id]()
    {
      auto newNow = std::chrono::high_resolution_clock::now();
      auto t0 = newNow;
      auto tid=id;
      std::ostringstream oss, osssc, binbdumpos;
      oss << "/tmp/error-blocks-" << tid << ".log";
      osssc << "/tmp/error-supchunksize-" << tid << ".log";
      binbdumpos <<  "/tmp/error-binblockdump-" << tid << ".log"; 
      std::ofstream errfile(oss.str());
      std::ofstream scfile(osssc.str());
      std::ofstream bdump(binbdumpos.str(), std::ios::binary);
      if (errfile.is_open()){}
      else { std::cout << "Couldn't open error dump file! \n"; }
      if (scfile.is_open()){}
      else { std::cout << "Couldn't open error dump file! \n";}
      if (bdump.is_open()){}
      else { std::cout << "Couldn't open error dump file! \n";}

	int numOps = 0;
	int newShorts = 0;
	int newChunks = 0;
	int newSubChunks = 0;
	int newBlocks = 0;
	int newErrorShorts = 0;
	int newErrorChunks = 0;
	int newErrorSubchunks = 0;
	int newErrorBlocks = 0;
	int newErrorsToReport = 0;

      unsigned xth = 0;
      while (true)
      {
        auto newNow = std::chrono::high_resolution_clock::now();

        numOps = m_packet_ctr.exchange(0);
        newShorts = m_short_ctr.exchange(0);
        newChunks = m_chunk_ctr.exchange(0);
        newSubChunks = m_subchunk_ctr.exchange(0);
        newBlocks = m_block_ctr.exchange(0);
        newErrorShorts = m_error_short_ctr.exchange(0);
        newErrorChunks = m_error_chunk_ctr.exchange(0);
        newErrorSubchunks = m_error_subchunk_ctr.exchange(0);
        newErrorBlocks = m_error_block_ctr.exchange(0);
        newErrorsToReport = m_errorsToReport.exchange(0);


        double seconds =  std::chrono::duration_cast<std::chrono::microseconds>(newNow-t0).count()/1000000.;

        bool fishy = false;
        bool errors = false;
        size_t pbs_count = m_q_prev_blocks.sizeGuess();
        size_t ebs_count = m_q_error_blocks.sizeGuess();
        if ( pbs_count>0 || ebs_count>0 )

        {
          if (pbs_count!=ebs_count)
          {
            fishy = true;
          }
          else
          {
            errors = true;
          }
        }
        cout_mutex.lock();
        std::cout << "Parser[" << tid << "] " << 
                  " BlockRate [kHz] : " << newBlocks/seconds/1000. << " PacketRate [kHz] : " << numOps/seconds/1000. <<  " newShorts:" <<
                  newShorts << " newChunks:" << newChunks << " newBlocks:" << newBlocks 
                  << " newErrorShorts:" << newErrorShorts << " newErrorChunks:" << newErrorChunks <<
                  " newErrorSubchunks:" << newErrorSubchunks
                  << " newErrorBlocks:" << newErrorBlocks << " errors?:" << errors << " sizeErr:" << newErrorsToReport <<
                  std::endl;
        cout_mutex.unlock();


#ifdef BLOCKDEBUG
        size_t numSupChunkSizeErr = m_error_chunk_sizes.sizeGuess();
        scfile << "ERROR SUPCHUNKSIZES from the last 5 seconds: ";
        for (unsigned i = 0; i<numSupChunkSizeErr; ++i)
        {
          uint32_t scsize;
          m_error_chunk_sizes.read(scsize);
          scfile << scsize << " , ";
        }
        scfile << '\n';


        if (errors)   // dump to link's file...
        {
          errfile << "### Statistics report at " << xth << "th. second, as " << newErrorBlocks <<
                  " present ###";
          for (unsigned i = 0; i<pbs_count; ++i)
          {
            felix::packetformat::block pb;
            felix::packetformat::block eb;
            m_q_prev_blocks.read(pb);
            m_q_error_blocks.read(eb);
 
            for (unsigned j = 0; j<BLOCKSIZE; ++j){
              bdump << ((uint8_t*)(&pb))[j]; 

            }
            for (unsigned j = 0; j<BLOCKSIZE; ++j){ 
              bdump << ((uint8_t*)(&eb))[j];
            }

            //netio::message bmsg((uint8_t*)&pb.data[0], BLOCKSIZE-4);
            //netio::message emsg((uint8_t*)&eb.data[0], BLOCKSIZE-4);
            //errfile << "\nPREV:" << blckheader_to_hex(pb)
            //        << '\n' << msg_to_hex(bmsg) << '\n' //<< m_prev_block << '\n'
            //        << "ERR :" << blckheader_to_hex(eb)
            //        << '\n' << msg_to_hex(emsg) << std::dec << "\n\n";
          }
          errfile << '\n';
        }
#endif

        xth+=5;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        t0 = newNow;
      }

    });
    statistics.detach();

#ifdef IOVEC
    for (size_t i=0 ; i < IOVEC_QUEUE; ++i)
    {
      m_vec[i] = new iovec[IOVEC];
    }
    m_vec_pos=1;
    m_vec_it = 0;
    m_num_chunks=0;
    m_total_size=0;
#endif

  }

  void log_packet(bool isShort)
  {
    m_packet_ctr++;
    if (isShort)
      m_short_ctr++;
    else
      m_chunk_ctr++;

    if(m_packet_ctr == m_packet_target)
    {
      m_packet_target *= 10;
    }
  }

  void dump_to_buffer(const char* data, long unsigned size, void* buffer, long unsigned buffer_pos)
  {
    long unsigned bytes_to_copy = size;
    while(bytes_to_copy > 0)
      {
        int n = std::min(bytes_to_copy, SUPERCHUNK_FRAME_SIZE-buffer_pos);
        memcpy(buffer+buffer_pos, data, n);
        buffer_pos += n;
        bytes_to_copy -= n;
        if(buffer_pos == SUPERCHUNK_FRAME_SIZE)
          {
            buffer_pos = 0;
          }
      }
  }

  void chunk_processed(const felix::packetformat::chunk& chunk)
  {
    uint32_t sumLength = 0;

    
    for (uint32_t i=0; i<chunk.subchunk_number(); ++i)
    {
      //uint32_t subchunk_length = chunk.subchunk_lengths()[i];
      sumLength += chunk.subchunk_lengths()[i];
      //std::cout << "CL:" << subchunk_length << " ";
    }
    //std::cout << '\n';
    //*/
    if ( sumLength!= SUPERCHUNK_SIZE )
    {
      error=true;
      m_error_chunk_sizes.write(sumLength);
      //cout_mutex.lock();
      //std::cout << "### BAD OMEN -> Received chunk size:" << sumLength << " Expected:" << SUPERCHUNK_SIZE << '\n';
      //cout_mutex.unlock();
      numErrors++;
      //cout_mutex.lock();
      //std::cout << m_id << " GOODC:"<< numGood << " ERRC:" << numErrors << ". bad sumLength:" << sumLength << '\n';
      //cout_mutex.unlock();
      numGood = 0;
    }
    else if ( sumLength==SUPERCHUNK_SIZE )
    {
      error = false; numGood++;
    }
    if (error)
    {
      //cout_mutex.lock();
      //std::cout << m_id << " RECOVERED after " << numErrors << '\n';
      //cout_mutex.unlock();
      m_errorsToReport+=numErrors;
      numErrors=0; error=false;
      return;
    }

    // publish chunk
#if defined(TAGPUBLISHER) && defined(IOVEC)
    //uint32_t subchunk_length;
    //uint32_t subchunk_number = chunk.subchunk_number();
    for (uint32_t i=0; i<subchunk_number; ++i)
    {
      subchunk_length = chunk.subchunk_lengths()[i];
      m_vec[m_vec_it][m_vec_pos].iov_base = const_cast<void*>(reinterpret_cast<const void *>
                                                              (chunk.subchunks()[i]));
      m_vec[m_vec_it][m_vec_pos].iov_len = subchunk_length;
      m_vec_pos++;
      m_total_size+=subchunk_length;
    }
    m_num_chunks++;
    if (m_num_chunks >= IOVEC/4-5)
    {
      flush_iovec();
    }
#elif defined(TAGPUBLISHER)
    netio::message msg((uint8_t**)chunk.subchunks(), chunk.subchunk_lengths(), chunk.subchunk_number());
    SUPERCHUNK_CHAR_STRUCT superchunk;
    msg.serialize_to_usr_buffer((void*)&superchunk);    
    //m_tagPub->publish(msg);
#elif defined(DUMP_SUPCHUNKS) //DUMP_SUPCHUNKS
    SUPERCHUNK_CHAR_STRUCT superchunk;
    long unsigned bytes_copied_chunk = 0;
    auto subchunk_data = chunk.subchunks();
    auto subchunk_sizes = chunk.subchunk_lengths();
    unsigned n_subchunks = chunk.subchunk_number();
    for(unsigned i=0; i<n_subchunks; i++)
      {
        dump_to_buffer(subchunk_data[i], subchunk_sizes[i], (void*)&superchunk, bytes_copied_chunk);
        bytes_copied_chunk += subchunk_sizes[i];
      }
    m_pcq->write( std::move(superchunk) );
#else
    netio::message msg((uint8_t**)chunk.subchunks(), chunk.subchunk_lengths(), chunk.subchunk_number());
    SUPERCHUNK_CHAR_STRUCT superchunk;
    msg.serialize_to_usr_buffer((void*)&superchunk);
    m_pcq->write( std::move(superchunk) ); 
//    m_pub->publish(0, msg);
#endif
    log_packet(false);
  }


  void shortchunk_processed(const felix::packetformat::shortchunk& chunk)
  {
    // publish shortchunk
#if defined(TAGPUBLISHER) && defined(IOVEC)
    m_vec[m_vec_it][m_vec_pos].iov_base = const_cast<void*>(reinterpret_cast<const void*>(chunk.data));
    m_vec[m_vec_it][m_vec_pos].iov_len = chunk.length;
    m_vec_pos++;
    m_total_size+=chunk.length;
    m_num_chunks++;
    if (m_num_chunks >= IOVEC/4-5)
    {
      flush_iovec();
    }
#elif defined(TAGPUBLISHER)
    netio::message msg((uint8_t*)chunk.data, chunk.length);
    m_tagPub->publish(msg);
#else
    //netio::message msg((uint8_t*)chunk.data, chunk.length);
    std::cout << " SHORTCHUNK IS NOT HANDLED AND IT SHOULD NOT HAPPEN YAY! \n";
    //m_pub->publish(0, msg);
#endif
    log_packet(true);
  }

  void subchunk_processed(const felix::packetformat::subchunk& subchunk) {
     m_subchunk_ctr++;
  }
  void block_processed(const felix::packetformat::block& block)
  {
#if defined(BLOCKDEBUG)
    m_prev_block = block;
#endif
    m_block_ctr++;
  }

  void chunk_processed_with_error(const felix::packetformat::chunk&)
  {
    m_error_chunk_ctr++;
  }

  void subchunk_processed_with_error(const felix::packetformat::subchunk& subchunk)
  {
    std::cout << " SUBCHUNK_ERROR: " << subchunktrailer_to_hex(subchunk) << '\n';
    m_error_subchunk_ctr++;
  }

  void shortchunk_process_with_error(const felix::packetformat::shortchunk& chunk)
  {
    m_error_short_ctr++;
  }

  void block_processed_with_error(const felix::packetformat::block& block)
  {
#if defined(BLOCKDEBUG)
    m_err_block = block;
    m_q_prev_blocks.write(m_prev_block);
    m_q_error_blocks.write(m_err_block);
#endif
//inline std::string blckheader_to_hex
    std::cout << "BLOCK ERROR: " << blckheader_to_hex(block) << '\n';
    m_error_block_ctr++;
  }
};


class ProtoDuneDispatcher
{
private:
  typedef felix::packetformat::BlockParser<ProtoDuneParserOps> ProtoDuneParser;
  bool m_running;
  unsigned m_cardId;
  unsigned m_linkId;
  unsigned m_queueId;
  std::atomic<unsigned> m_qsize;
  //unsigned m_readVirtAddr; //eg180615 for GLM: this was not defined.. was it an atomic?
  ProtoDuneParserOps m_parser_ops;
  ProtoDuneParser m_parser;
  folly::ProducerConsumerQueue<uint64_t> m_queue;
  //boost::lockfree::spsc_queue<uint64_t, boost::lockfree::capacity<1024*1024> > m_queue;
public:
  ProtoDuneDispatcher(unsigned card, unsigned link, unsigned tag, unsigned queueId )://std::unique_ptr<netio::publish_socket>& socket) :
    m_running(false),
    m_cardId(card),
    m_linkId(link),
    m_queueId(queueId),
    m_parser_ops(tag, link, queueId),
    m_parser(m_parser_ops),
    m_queue(6000000)
  {

    unsigned id = link;
    std::thread statistics([this, id]()
    {
      auto newNow = std::chrono::high_resolution_clock::now();
      auto t0 = newNow;
      auto tid=id;
      while (true)
      {
        auto newNow = std::chrono::high_resolution_clock::now();
        cout_mutex.lock();
        std::cout << "Dispatcher[" << tid << "] Write says -> QueueSize:" << m_qsize << std::endl;
        cout_mutex.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        t0 = newNow;
      }
    });
    statistics.detach();

  }
  void run()
  {
    int expected = -1;
    while (m_running)
    {
      uint64_t block_addr;
      if (m_queue.read( block_addr ))
      {
        const felix::packetformat::block* block = const_cast<felix::packetformat::block*>
                                                  (felix::packetformat::block_from_bytes((const char*)block_addr));
        if (expected >= 0) {
          if (expected != block->seqnr){
            //std::cout << "WOOF WOOF --> link:" << m_linkId << " Not expected seqn! Expected:" << expected << " but got:" << block->seqnr << std::endl;     
          }
        }
        expected = (block->seqnr + 1) % 32;       

        m_parser.process(block);
      }
      else
      {
        // no data to process, sleep a bit
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    }
  }

  void isRunning(bool isRunning)
  {
    m_running = isRunning;
  }
  /*
  		uint64_t lastVirtReadAddr() {
  			return m_readVirtAddr;
  		}
  */
  void queue(uint64_t block_addr)
  {
    //blocking call: do not allow the felix reader to continue if we can't process the data fast enough!
    static uint64_t failures=0;
    static uint64_t failures_interval=1;
    m_qsize = m_queue.sizeGuess();
    if ( !m_queue.write(block_addr) )
    {
      failures++;
      if(failures > 0 && failures%failures_interval == 0)
      {
        std::cout << "Dispatcher queue for elink " << m_linkId << " full!!! " << failures <<
                  " dropped blocks.";
        failures_interval *=100;
      }
    }
  }

};


std::map<unsigned, std::unique_ptr<FlxCard>> m_flxCards;
std::mutex card_mutex;

class ProtoDuneReader
{
private:
  std::atomic<bool> m_running;
  //FlxCard m_flxCard;
  unsigned m_cardno;
  unsigned m_dmaid;
  int m_cmem_handle;            // handle to the DMA memory block
  uint64_t m_virt_addr;           // virtual address of the DMA memory block
  uint64_t m_phys_addr;           // physical address of the DMA memory block
  uint64_t m_currentAddress;      // pointer to the current write position for the card
  unsigned m_read_index;
  u_long dst;

  ////////////////
  //std::unique_ptr<felix::core::CircularDMA> circular_dma;
  ///////////////

  std::map<uint32_t, uint32_t>& m_linkInfos;
  std::vector< std::unique_ptr<ProtoDuneDispatcher>> m_dispatchers;
  std::thread m_dispatcherThreads[6]; // max 6 links per DMA!
  std::atomic<int> m_go{0};

public:
  ProtoDuneReader(unsigned cardno, unsigned dmaid,
                  std::map<uint32_t, uint32_t>& linkInfos )
    : m_cardno(cardno), m_dmaid(dmaid), m_linkInfos(linkInfos)
  {

    unsigned id = cardno + dmaid;

    std::cout << "INSERT UNIQUE FLXCARD FOR CARD: " << m_cardno << '\n';
    m_flxCards[m_cardno] = std::make_unique<FlxCard>();

    std::thread statistics([this, id]()
    {
      double buffSize = PROTODUNE_MEMSIZE; ///1024/1024;

      auto newNow = std::chrono::high_resolution_clock::now();
      auto t0 = newNow;
      auto tid=id;
      auto countStart = 0;
      while (true)
      {
        auto newNow = std::chrono::high_resolution_clock::now();
        cout_mutex.lock();
        unsigned bava = bytesAvailable();
        countStart++;
        if (countStart == 10) { m_go=1; }
        std::cout << "CardReader[" << tid << "] BytesAvailable:" << bava << " " <<
                  (bava/buffSize)*100. << "%" << std::endl;
        cout_mutex.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        t0 = newNow;
      }
    });
    statistics.detach();


    // We have 1 ProtoDUNE reader per DMA, but many internal reader threads, one for each link

    //for (auto link : linkInfos) {
    std::map<uint32_t, uint32_t>::iterator link;
    for ( link = linkInfos.begin(); link != linkInfos.end(); link++ )
    {
      std::cout << "DISPATCHER FOR LINK " << link->first << '\n';


      unsigned lId = link->first;
      if (link->second > 1000) { lId+=5; }
      m_dispatchers.emplace_back( std::make_unique<ProtoDuneDispatcher>(m_cardno, lId, link->second, link->first) );
      //ProtoDuneDispatcher *p = new  ProtoDuneDispatcher(cardno, link.first, link.second);
      //m_dispatchers[link.first] = p;
    }

    try
    {
      std::cout <<"Opening CARD " << m_cardno << '\n';
      card_mutex.lock();
      m_flxCards[m_cardno]->card_open(m_cardno, LOCK_NONE);
      card_mutex.unlock();
    }
    catch(FlxException ex)
    {
      std::cout << "Exception thrown: " << ex.what() << '\n';
      exit(EXIT_FAILURE);
    }
    std::cout <<"CARD Opened" << '\n';

    m_cmem_handle = dma_allocate_cmem(PROTODUNE_MEMSIZE, &m_phys_addr, &m_virt_addr);
    std::cout <<"CMEM circular buffer allocated (" << PROTODUNE_MEMSIZE/1024/1024 << " MB)" << '\n';

    card_mutex.lock();
    m_flxCards[m_cardno]->dma_stop(m_dmaid);
    std::cout <<"flxCard.dma_stop issued." << '\n';

    //m_flxCards[m_cardno]->irq_disable();
    //std::cout <<"flxCard.irq_disable issued.");

    m_flxCards[m_cardno]->dma_reset();
    std::cout <<"flxCard.dma_reset issued." << '\n';

    m_flxCards[m_cardno]->soft_reset();
    std::cout <<"flxCard.soft_reset issued." << '\n';

    //flxCard.irq_disable(ALL_IRQS);
    //std::cout <<"flxCard.irq_diable(ALL_IRQS) issued.");

    m_flxCards[m_cardno]->dma_fifo_flush();
    std::cout <<"flxCard.dma_fifo_flush issued." << '\n';

    //m_flxCards[m_cardno]->irq_enable(IRQ_DATA_AVAILABLE);
    //std::cout <<"flxCard.irq_enable(IRQ_DATA_AVAILABLE) issued.");

    card_mutex.unlock();


    ///////////////////
    //circular_dma = std::make_unique<felix::core::CircularDMA>(*m_flxCards[m_cardno], m_dmaid, PROTODUNE_MEMSIZE);
    ///////////////////



    m_currentAddress = m_phys_addr;
    dst = m_phys_addr;
    m_read_index = 0;

    CPUPin& cpup = CPUPin::getInstance();
    for (uint32_t i = 0; i<m_dispatchers.size(); ++i)
    {
      m_dispatchers[i]->isRunning(true);
      cpu_set_t cpuset = cpup.getDispatcherCPUs(712, m_cardno, i);
      m_dispatcherThreads[i] = std::thread(&ProtoDuneDispatcher::run, m_dispatchers[i].get());
      int rc0 = pthread_setaffinity_np(m_dispatcherThreads[i].native_handle(), sizeof(cpu_set_t),
                                       &cpuset);
      std::cout <<"Dispatcher pinned.... RC:" << rc0 << '\n';
      //++i;
      set_thread_name(m_dispatcherThreads[i], "disp", i);
    }
  }


  int dma_allocate_cmem(u_long bsize, u_long* paddr, u_long* vaddr)
  {
    int handle;
    int ret = CMEM_Open();
    if (!ret)
      ret = CMEM_GFPBPASegmentAllocate(bsize, (char*)"FelixCore", &handle);

    if (!ret)
      ret = CMEM_SegmentPhysicalAddress(handle, paddr);

    if (!ret)
      ret = CMEM_SegmentVirtualAddress(handle, vaddr);

    if (ret)
    {
      rcc_error_print(stdout, ret);
      std::cout << "You either allocated to little CMEM memory or run felixcore demanding too much CMEM memory." << '\n';
      std::cout << "Fix the CMEM memory reservation in the driver or run felixcore with the -m option." << '\n';
      card_mutex.lock();
      m_flxCards[m_cardno]->card_close();
      card_mutex.unlock();
      exit(EXIT_FAILURE);
    }

    return handle;
  }


  void isRunning(bool r)
  {
    m_running = r;
  }

  inline uint64_t bytesAvailable()
  {
    return (m_currentAddress - ((m_read_index * BLOCKSIZE) + m_phys_addr) + PROTODUNE_MEMSIZE) %
           PROTODUNE_MEMSIZE;
  }

  inline void read_current_addr()
  {
    card_mutex.lock();
    m_currentAddress = m_flxCards[m_cardno]->m_bar0->DMA_DESC_STATUS[m_dmaid].current_address;
    card_mutex.unlock();
  }

  void startRunning()
  {
    std::cout <<"issuing flxCard.dma_to_host for card " << m_cardno << " dma id:" << m_dmaid << '\n';
    card_mutex.lock();
    m_flxCards[m_cardno]->dma_to_host(m_dmaid, m_phys_addr, PROTODUNE_MEMSIZE, FLX_DMA_WRAPAROUND);
    card_mutex.unlock();
    std::cout <<"flxCard.dma_to_host issued for card " << m_cardno << "." << '\n';
  }

/* ///////////////////
  bool read() {
    circular_dma->wait(BLOCKSIZE);
    size_t bytes_to_consume = circular_dma->bytes_available_to_read();
    bytes_to_consume -= bytes_to_consume % BLOCKSIZE;
    if(bytes_to_consume % BLOCKSIZE != 0){
      ERROR("DMA transfer truncated");
      return false;
    }

    for(unsigned i = 0; i < bytes_to_consume; i += BLOCKSIZE)
    {
      uint64_t fromAddress = circular_dma->longPtr(); 
      // CHECKS ON THIS IF NEEDED!
      const felix::packetformat::block* block = felix::packetformat::block_from_bytes((const char*)circular_dma->ptr()+i);

      //if (!blockChecker->checkSignature(block)) {
      //  ERROR("No Valid Signature found in block");
      //  //dump_buffer((u_long)block, 1024);
      //  return false;
      // }
      //int expected = blockChecker->checkSeqNo(block);
      //if (expected >= 0) {
      //  ERROR("Wrong Sequence Number found in block, found " << block->seqnr <<
      //        " instead of  " << expected <<
      //        " for elink " << hex(block->elink));
        //dump_buffer((u_long)block, 1024);
      //}

      // NEED TO DISPATCH BLOCK.
      m_dispatchers[block->elink/64]->queue( fromAddress );
    }
    circular_dma->consume(bytes_to_consume);
    return true;
  }

  void run_dma()
  { 
    this->isRunning(true);
    uint64_t cno = m_cardno*1024;
    sleep(2);

    try {
      bool ok = true;
      while(ok) {
        ok = read();
      }
      ERROR("Reader Thread died");
    } catch (std::exception& e) {
      CRITICAL("Uncaught exception in source thread: " << e.what());
      throw;
    }   
  }
*/ ////////////////////////////

  void run()
  {
    this->isRunning(true);
    while (m_running)
    {
      // Loop while there are not enough data
      while((m_currentAddress < m_phys_addr) || (m_phys_addr+PROTODUNE_MEMSIZE < m_currentAddress))
      {
        read_current_addr();
        std::this_thread::sleep_for(std::chrono::microseconds(5000)); //cfg.poll_time = 5000
      }

      while (bytesAvailable() < BLOCK_THRESHOLD*BLOCKSIZE)
      {
        std::this_thread::sleep_for(std::chrono::microseconds(5000)); // cfg.poll_time = 5000
        read_current_addr();
      }

      u_long write_index = (m_currentAddress - m_phys_addr) / BLOCKSIZE;

      uint64_t bytes = 0;
      while (m_read_index != write_index)
      {
        uint64_t fromAddress = m_virt_addr + (m_read_index * BLOCKSIZE);
        const felix::packetformat::block* block = const_cast<felix::packetformat::block*>
                                                  (felix::packetformat::block_from_bytes((const char*)fromAddress));

        //std::cout << std::dec << "CNO " << cno << " BLOCK ELINK -> " << block->elink/64 << '\n';
        //uint32_t elink = block->elink/64;
        //if ( !(elink==0 || elink==1 || elink==2 || elink==3 || elink==4 || elink==5) ){
        //  std::cout << std::dec << " CARD: " << cno << " BLOCK ELINK -> " << elink << " HEXNODIV:" << std::hex << block->elink << '\n';
        //  continue;
        //}
        //

        m_dispatchers[block->elink/64]->queue( fromAddress ); 
        //parser.process(block);

        m_read_index = (m_read_index + 1) % (PROTODUNE_MEMSIZE / BLOCKSIZE);
        bytes += BLOCKSIZE;
      }

      // here check if we can move the read pointer in the circ buf!!
      //uint64_t dst = m_phys_addr + (write_index*BLOCKSIZE) - (MARGIN_BLOCKS*BLOCKSIZE);
      dst = m_phys_addr + (write_index*BLOCKSIZE) - (MARGIN_BLOCKS*BLOCKSIZE);
      if (dst < m_phys_addr)
      {
        dst += PROTODUNE_MEMSIZE;
      }
      card_mutex.lock();
      m_flxCards[m_cardno]->dma_set_ptr(m_dmaid, dst);
      card_mutex.unlock(); 
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Wait that threads are done
    for (uint32_t i = 0; i<m_dispatchers.size(); ++i)
    {
      m_dispatchers[i]->isRunning(false);
    }
    for (uint32_t i = 0; i<m_dispatchers.size(); ++i)
    {
      m_dispatcherThreads[i].join();
    }
  }
};


