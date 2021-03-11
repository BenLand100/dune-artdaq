#pragma once

//#include "DFDebug/DFDebug.h"


namespace felix
{
namespace core
{

class CMEMBuffer
{
private:
    int handle;
    u_long paddr_;
    u_long vaddr_;
    size_t size_;

public:
    CMEMBuffer() : paddr_(0), vaddr_(0), size_(0) {}

    CMEMBuffer(size_t size)
    {
      this->size_ = size;

      int ret = CMEM_Open();

      if (!ret)
        ret = CMEM_GFPBPASegmentAllocate(size, (char*)"FelixCore", &handle);

      if (!ret)
        ret = CMEM_SegmentPhysicalAddress(handle, &paddr_);

      if (!ret)
        ret = CMEM_SegmentVirtualAddress(handle, &vaddr_);

      if (ret)
      {
        rcc_error_print(stdout, ret);
        std::cout << "CMEM CMEM CMEM CMEM ALLOC FAILED." << '\n';
        //WARNING("You either allocated to little CMEM memory or run felixcore demanding too much CMEM memory.");
        //WARNING("Fix the CMEM memory reservation in the driver or run felixcore with the -m option.");
        throw std::runtime_error("CMEM memory allocation failed");
      }
    }


    ~CMEMBuffer()
    {
      int ret = CMEM_SegmentFree(handle);
      if (!ret)
        ret = CMEM_Close();
      if (ret)
        rcc_error_print(stdout, ret);
    }


    inline u_long paddr() const { return paddr_; }
    inline u_long vaddr() const { return vaddr_; }
    inline size_t size() const { return size_; }
};

}
}
