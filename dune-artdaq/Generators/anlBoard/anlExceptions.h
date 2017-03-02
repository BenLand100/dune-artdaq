#ifndef ANLEXCEPTIONS_H__
#define ANLEXCEPTIONS_H__

#include <stdexcept>

namespace SSPDAQ{

  //================================//
  //Bad requests from Device Manager//
  //================================//

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

  //=======================================//
  //Error reported by USB interface library//
  //=======================================//

  class EFTDIError: public std::runtime_error{
  public:
    explicit EFTDIError(const std::string &s):
      std::runtime_error(s) {}

    explicit EFTDIError():
      std::runtime_error("") {}
  };

  //=======================================//
  //Error reported by TCP interface library//
  //=======================================//

  class ETCPError: public std::runtime_error{
  public:
    explicit ETCPError(const std::string &s):
      std::runtime_error(s) {}

    explicit ETCPError():
      std::runtime_error("") {}
  };


  //===============================================//
  //Error receiving expected event data from device//
  //===============================================//

  class EEventReadError: public std::runtime_error{
  public:
    explicit EEventReadError(const std::string &s):
      std::runtime_error(s) {}

    explicit EEventReadError():
      std::runtime_error("") {}
  };

  class EDAQConfigError: public std::runtime_error{
  public:
    explicit EDAQConfigError(const std::string &s):
      std::runtime_error(s) {}

    explicit EDAQConfigError():
      std::runtime_error("") {}
  };

}//namespace
#endif
