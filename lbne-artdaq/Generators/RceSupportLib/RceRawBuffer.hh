/*
 * RceRawBuffer.hh
 *
 *  Created on: May 21, 2014
 *      Author: tcn45
 */

#ifndef RCERAWBUFFER_HH_
#define RCERAWBUFFER_HH_

#include <iostream>

namespace lbne
{

  class RceRawBuffer {

  public:
    RceRawBuffer(size_t size) :
    data_(new std::vector<uint8_t>(size)),
    dataPtr_(&*(data_->begin())),
    size_(size),
    flags_(0)
    { }

    RceRawBuffer(uint8_t* dataPtr, size_t size) :
    data_(0),
    dataPtr_(dataPtr),
    size_(size),
    flags_(0)
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

    size_t size(void) { return size_; }
    uint32_t flags(void) { return flags_; }
    uint8_t* dataPtr(void) { return dataPtr_; }

  private:
    std::shared_ptr<std::vector<uint8_t> > data_;
    uint8_t* dataPtr_;
    size_t size_;
    uint32_t flags_;

  };

  typedef std::unique_ptr<lbne::RceRawBuffer> RceRawBufferPtr;

} /* namespace lbne */

#endif /* RCERAWBUFFER_HH_ */
