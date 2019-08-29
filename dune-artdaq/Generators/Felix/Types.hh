#ifndef TYPES_HH_
#define TYPES_HH_

#include "packetformat/block_format.hpp"

#include "ProducerConsumerQueue.hh"
#include "NetioWIBRecords.hh"
#include "netio/netio.hpp"

/*
 * Types
 * Author: Roland.Sipos@cern.ch
 * Description: 
 *   Collection of custom types used in the FELIX BR.
 * Date: May 2018
 */

typedef folly::ProducerConsumerQueue<SUPERCHUNK_CHAR_STRUCT> FrameQueue;
typedef std::unique_ptr<FrameQueue> UniqueFrameQueue;

typedef folly::ProducerConsumerQueue<felix::packetformat::block> BlockQueue;
typedef std::unique_ptr<BlockQueue> UniqueBlockQueue;

typedef folly::ProducerConsumerQueue<netio::message> MessageQueue;
typedef std::unique_ptr<MessageQueue> UniqueMessageQueue;

#endif

