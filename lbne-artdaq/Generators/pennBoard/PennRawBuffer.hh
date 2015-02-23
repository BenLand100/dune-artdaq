/*
 * PennRawBuffer.hh
 *
 *  Created on: Dec 15, 2014
 *      Author: tdealtry (based on tcn45 rce code)
 */

#ifndef PENNRAWBUFFER_HH_
#define PENNRAWBUFFER_HH_

#include <iostream>

namespace lbne
{

  class PennRawBuffer {

  public:
    PennRawBuffer(size_t size) :
      data_(new std::vector<uint8_t>(size)),
      dataPtr_(&*(data_->begin())),
      size_(size),
      flags_(0),
      count_(0)
    { }

    PennRawBuffer(uint8_t* dataPtr, size_t size) :
      data_(0),
      dataPtr_(dataPtr),
      size_(size),
      flags_(0),
      count_(0),
      count_payload_(0),
      count_payload_counter_(0),
      count_payload_trigger_(0),
      count_payload_timestamp_(0)
    { }

    ~PennRawBuffer()
    { }

    void setSize(size_t size)
    {
      if (data_ != 0)
      {
        data_->resize(size);
      }
      size_ = size;
    }

    void setFlags(uint32_t flags) { flags_ = flags; }
    void setCount(uint32_t count) { count_ = count; }
    void setCountPayload         (uint16_t count_payload          ) { count_payload_           = count_payload; }
    void setCountPayloadCounter  (uint16_t count_payload_counter  ) { count_payload_counter_   = count_payload_counter; }
    void setCountPayloadTrigger  (uint16_t count_payload_trigger  ) { count_payload_trigger_   = count_payload_trigger; }
    void setCountPayloadTimestamp(uint16_t count_payload_timestamp) { count_payload_timestamp_ = count_payload_timestamp; }

    size_t   size(void)    { return size_; }
    uint32_t flags(void)   { return flags_; }
    uint32_t count(void)   { return count_; }
    uint16_t countPayload         (void)   { return count_payload_; }
    uint16_t countPayloadCounter  (void)   { return count_payload_counter_; }
    uint16_t countPayloadTrigger  (void)   { return count_payload_trigger_; }
    uint16_t countPayloadTimestamp(void)   { return count_payload_timestamp_; }
    uint8_t* dataPtr(void) { return dataPtr_; }

  private:
    std::shared_ptr<std::vector<uint8_t> > data_;
    uint8_t* dataPtr_;
    size_t   size_;
    uint32_t flags_;
    uint32_t count_;
    uint16_t count_payload_;
    uint16_t count_payload_counter_;
    uint16_t count_payload_trigger_;
    uint16_t count_payload_timestamp_;

  };

  typedef std::unique_ptr<lbne::PennRawBuffer> PennRawBufferPtr;

} /* namespace lbne */

#endif /* PENNRAWBUFFER_HH_ */
