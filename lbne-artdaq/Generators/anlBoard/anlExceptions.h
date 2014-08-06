#ifndef ANLEXCEPTIONS_H__
#define ANLEXCEPTIONS_H__

#include <stdexcept>

namespace SSPDAQ{

  class ENoSuchDevice: public std::runtime_error{
  public:
    explicit ENoSuchDevice(const std::string &s):
      std::runtime_error(s) {}

    explicit ENoSuchDevice():
      std::runtime_error("") {}
  };

  class EDeviceAlreadyOpen: public std::runtime_error{
  public:
    explicit EDeviceAlreadyOpen(const std::string &s):
      std::runtime_error(s) {}

    explicit EDeviceAlreadyOpen():
      std::runtime_error("") {}

  };

  class EBadDeviceList: public std::runtime_error{
  public:
    explicit EBadDeviceList(const std::string &s):
      std::runtime_error(s) {}
      
      explicit EBadDeviceList():
	std::runtime_error("") {}

  };

  class EFTDIError: public std::runtime_error{
  public:
    explicit EFTDIError(const std::string &s):
      std::runtime_error(s) {}

    explicit EFTDIError():
      std::runtime_error("") {}

  };


}//namespace
#endif
