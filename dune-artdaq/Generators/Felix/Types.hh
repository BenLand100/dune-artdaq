#ifndef TYPES_HH_
#define TYPES_HH_

#include "ProducerConsumerQueue.hh"
#include "NetioWIBRecords.hh"
#include "netio/netio.hpp"
#include "TriggerPrimitive/frame_expand.h"

/*
 * Types
 * Author: Roland.Sipos@cern.ch
 * Description: 
 *   Collection of custom types used in the FELIX BR.
 * Date: May 2018
 */

typedef folly::ProducerConsumerQueue<SUPERCHUNK_CHAR_STRUCT> FrameQueue;
typedef std::unique_ptr<FrameQueue> UniqueFrameQueue;

typedef folly::ProducerConsumerQueue<netio::message> MessageQueue;
typedef std::unique_ptr<MessageQueue> UniqueMessageQueue;

typedef folly::ProducerConsumerQueue<MessageCollectionADCs> CollectionQueue;
typedef std::unique_ptr<CollectionQueue> UniqueCollectionQueue;
#endif

