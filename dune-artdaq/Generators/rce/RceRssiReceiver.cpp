#include "RceRssiReceiver.hh"

#include <cmath>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

#include "dam/DataFragmentUnpack.hh"

namespace dune{
namespace rce {

RssiSink::RssiSink(RssiReceiver *receiver) :
   _receiver(receiver)
{
}

void RssiSink::
     acceptFrame (boost::shared_ptr<rogue::interfaces::stream::Frame> frame ) 
{
   while (_receiver->_paused.load())
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));

   auto stats_local = stats.load();

   auto nbytes = frame->getPayload();
   stats_local.rx_last = nbytes;
   stats_local.rx_bytes += nbytes;
   ++stats_local.rx_cnt;

   stats_local.rssi_drop = _receiver->_rssi->getDropCount();
   stats_local.pack_drop = _receiver->_pack->getDropCount();

   float dt      = 0;
   auto start    = boost::posix_time::second_clock::local_time();
   auto timeout  = _receiver->_buffer_timeout;
   auto& buffers = _receiver->_buffers;

   while (buffers.write_available() == 0 && dt < timeout)
   {
      boost::this_thread::sleep(boost::posix_time::milliseconds(10));
      auto diff = boost::posix_time::second_clock::local_time() - start;
      dt = diff.total_milliseconds();
   }

   // copy data to buffer
   if (buffers.write_available() > 0)
   {
      typedef artdaq::Fragment::byte_t byte_t;

      // FIXME
      nbytes += 12;
      //nbytes += sizeof(uint64_t);

      // calucate number of words for artDAQ::Fragment
      double size_of_word = static_cast<double>(sizeof(artdaq::RawDataType));
      size_t n_words = ceil(nbytes / size_of_word);

      auto* buf = new Buffer(n_words);

      // FIXME
      size_t padding = 12;
      memset(buf->dataBeginBytes(), 0, padding * sizeof(byte_t));
     
      // align to the next 64-bit word
      /*
      size_t padding     = 0;
      size_t n8          = buf->dataBeginBytes() - buf->headerBeginBytes();
      size_t bytes_mod64 = (n8 * sizeof(byte_t)) % sizeof(uint64_t);

      if ( bytes_mod64 > 0) {
         size_t bytes_pad = sizeof(uint64_t) - bytes_mod64;
         padding = bytes_pad / sizeof(byte_t);
         memset(buf->dataBeginBytes(), 0, padding * sizeof(byte_t));
      }
      */

      auto *dst = buf->dataBeginBytes() + padding;
      auto iter = frame->beginRead();
      auto end  = frame->endRead();

      while (iter != end) {
         auto nxt  = iter.endBuffer ();
         auto size = iter.remBuffer ();
         auto *src = iter.ptr       ();
         memcpy(dst, src, size);
         dst += size;
         iter = nxt;
      }

      auto *data_ptr = buf->dataBeginBytes();
      auto *header = reinterpret_cast<uint64_t *>(data_ptr + padding);
      auto timestamp = *(header + 2);
      buf->setTimestamp  ( timestamp );

      size_t   n64     = (*header >> 8) & 0xffffff;
      uint64_t trailer =  *(header + n64 - 1);
      bool     is_okay = true; 

      // check header
      if (*header >> 40 != 0x8b309e) {
         ++stats_local.bad_hdrs;
         is_okay = false;
      }
      else {
         // check trailer
         if (trailer != ~*header) {
            ++stats_local.bad_trlr;
            is_okay = false;
         }

         // check size
         if (nbytes != n64 * sizeof(uint64_t) + padding) {
            ++stats_local.err_size;
            is_okay = false;
         }
      }

      // check TpcStream
      if (is_okay) {
         DataFragmentUnpack data(header);
         if (!data.isTpcNormal())
            is_okay = false;
      }

      if (!is_okay)
         ++stats_local.err_cnt;

      buffers.push(buf);
   }
   else {
      ++stats_local.overflow;
   }

   // sync stats
   stats.store(stats_local);
}

RssiReceiver::
   RssiReceiver(std::string ip, uint16_t port, uint16_t nframes) : 
      _sink(boost::make_shared<RssiSink>(this)),
      _paused(false)
{
   // Create the UDP client, jumbo = true
   _udp  = rogue::protocols::udp::Client::create(ip.c_str(), port, true);

   // Make enough room for 'nframes' outstanding buffers
   _udp->setRxBufferCount (nframes); 

   // RSSI
   _rssi = rogue::protocols::rssi::Client::create(_udp->maxPayload());

   // Packetizer, ibCrc = false, obCrc = true
   ///_pack = rogue::protocols::packetizer::CoreV2::create(false,true);
   _pack = rogue::protocols::packetizer::Core::create();

   // Connect the RSSI engine to the UDP client
   _udp ->setSlave  (_rssi->transport ());
   _rssi->transport ()->setSlave   (_udp);

   // Connect the RSSI engine to the packetizer
   _rssi->application ()->setSlave (_pack->transport());
   _pack->transport   ()->setSlave (_rssi->application());

   // Connect the sink to channel 0 of the packetizer
   _pack->application(0)->setSlave (_sink);
}

RssiReceiver::~RssiReceiver()
{
   // no resume
   pause_and_clear(false);
}

bool RssiReceiver::pop(BufferPtr &buf)
{
   return _buffers.pop(buf);
}

RecvStats RssiReceiver::get_stats() const
{
   return _sink->stats.load();
}

size_t RssiReceiver::set_buffer_size(size_t size)
{
   if (size == 0 || size > MAX_BUFFER_SIZE)
      _buffer_size = MAX_BUFFER_SIZE;
   else
      _buffer_size = size;

   return size;
}

size_t RssiReceiver::set_buffer_timeout(size_t timeout)
{
   _buffer_timeout = timeout;
   return _buffer_timeout;
}

size_t RssiReceiver::pause_and_clear(bool resume)
{
   _paused.store(true);

   // delete uncomsumed buffer
   size_t n = 0;
   _buffers.consume_all( [&n](BufferPtr buf) { delete buf; ++n; });

   if (resume)
      _paused.store(false);

   return n;
}

}} // namespace dune::rce
