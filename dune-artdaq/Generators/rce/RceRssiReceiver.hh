#ifndef RCERSSIRECEIVER_HH_
#define RCERSSIRECEIVER_HH_

#include <string>

#include <rogue/protocols/udp/Core.h>
#include <rogue/protocols/udp/Client.h>
#include <rogue/protocols/rssi/Client.h>
#include <rogue/protocols/rssi/Transport.h>
#include <rogue/protocols/rssi/Application.h>
#include <rogue/protocols/packetizer/CoreV2.h>
#include <rogue/protocols/packetizer/Core.h>
#include <rogue/protocols/packetizer/Transport.h>
#include <rogue/protocols/packetizer/Application.h>
#include <rogue/interfaces/stream/Frame.h>
#include <rogue/interfaces/stream/FrameIterator.h>
#include <rogue/interfaces/stream/Buffer.h>
#include <rogue/Logging.h>

#include <boost/lockfree/spsc_queue.hpp>

#include "artdaq-core/Data/Fragment.hh"

namespace dune {
namespace rce {

   typedef artdaq::Fragment                    Buffer;
   typedef artdaq::Fragment*                   BufferPtr;

   class RssiReceiver;

   class RecvStats
   {
      public:
         uint32_t rx_count;
         uint64_t rx_bytes;
         uint32_t rx_last;
         uint32_t pack_drop;
         uint32_t rssi_drop;
         uint32_t buf_overflow;
   };

   class RssiSink: public rogue::interfaces::stream::Slave 
   {
      public:
         RssiSink(RssiReceiver *receiver);

         void acceptFrame (boost::shared_ptr<rogue::interfaces::stream::Frame> frame);

         boost::atomic<RecvStats> stats;

      private:
         RssiReceiver *_receiver;
   };

   class RssiReceiver
   {
      friend class RssiSink;
     public:
         RssiReceiver(std::string ip, uint16_t port, uint16_t nframes=64);
         ~RssiReceiver();

         RecvStats get_stats() const;

         size_t pause_and_clear(bool resume);

         bool pop(BufferPtr &buf); 

         size_t set_buffer_size    (size_t size   );
         size_t set_buffer_bytes   (size_t bytes  );
         size_t set_buffer_timeout (size_t timeout);

      private:
         // RSSI connection
         rogue::protocols::udp::ClientPtr       _udp;
         rogue::protocols::rssi::ClientPtr      _rssi;
         rogue::protocols::packetizer::CorePtr  _pack;
         boost::shared_ptr<RssiSink>            _sink;

         // Ring buffer
         static const int MAX_BUFFER_SIZE = 256;
         typedef boost::lockfree::spsc_queue<BufferPtr,
                 boost::lockfree::capacity<MAX_BUFFER_SIZE>> BufferQueue;

         BufferQueue _buffers;

         size_t _buffer_size       = 128;
         size_t _buffer_timeout    = 500; // ms

         boost::atomic<bool> _paused;
   };
}} // namespace dune::rce
#endif
