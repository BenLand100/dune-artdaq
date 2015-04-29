#ifndef lbne_artdaq_DAQLogger_DAQLogger_hh
#define lbne_artdaq_DAQLogger_DAQLogger_hh

#include "lbne-artdaq/RCConnection/RCConnection.hh"

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <stdexcept>
#include <string>

// JCF, 4/29/15

// The DAQLogger class is designed to allow users to make calls such as

// DAQLogger::LogError("mymodule") << "My error message";

// ...thereby replicating the feel of the messagefacility log messages, e.g.

// mf::LogError("mymodule") << "My error message";

// However, note that what DAQLogger::LogError(), etc., do is actually
// return a static member struct which contains an overload of the
// "<<" operator -- in the case of the LogError() call, it will return
// a struct which overloads "<<" such that it will call mf::LogError()
// with the error message, send the error message up to RunControl via
// the RCConnection singleton class, and throw an exception of type
// DAQLogger::ExceptClass, which inherits from std::exception and
// includes the error message. Similarly for the other LogXXXXX()
// calls, though the difference being that in their overloads of "<<"
// they will not throw an exception.

namespace lbne {

  class DAQLogger {

    // All data in DAQLogger is static, i.e., there's no reason to declare an instance of it
    DAQLogger() = delete; 
    
    struct Error;
    struct Warning;
    struct Info;
    struct Debug;
    struct Trace;

  public:
    static Error LogError(const std::string& caller_name) {
      caller_name_ = caller_name;
      return error_;
    }

    static Warning LogWarning(const std::string& caller_name) {
      caller_name_ = caller_name;
      return warning_;
    }

    static Info LogInfo(const std::string& caller_name) {
      caller_name_ = caller_name;
      return info_;
    }

    static Debug LogDebug(const std::string& caller_name) {
      caller_name_ = caller_name;
      return debug_;
    }

    static Trace LogTrace(const std::string& caller_name) {
      caller_name_ = caller_name;
      return trace_;
    }

  private:

    // This is the exception class which is automatically thrown on calls to DAQLogger::LogError()

    class ExceptClass : public std::exception {
    public:
      ExceptClass(const std::string& exceptstr) :
	exceptstr_(exceptstr) {}

      virtual const char* what() const noexcept {
	return exceptstr_.c_str();
      }

    private:
      std::string exceptstr_;
    };

    struct Error {
      void operator<<(const std::string& msg) {

	mf::LogError(caller_name_) << msg;

	RCConnection::Get().Send( caller_name_, msg );
	
	ExceptClass myexcept( msg );
	throw myexcept;
      }
    };

    // JCF, 4/29/15

    // As a guideline, it's usually a bad idea to create a macro --
    // however, here I'm going to be declaring four structs, each of
    // which defines its own stream operator "<<", so users can employ
    // DAQLogger::LogWarning(), etc., much like they can employ
    // mf::LogWarning(), etc., and each struct has the same form, so a
    // lot of potential typo-based errors and hassle can be prevented
    // by this macro

#define GENERATE_NONERROR_STREAMSTRUCT(NAME)               \
    struct NAME {                                          \
                                                           \
      void operator<<(const std::string& msg) {            \
                                                           \
	mf::Log ## NAME(caller_name_) << msg;              \
                                                           \
	RCConnection::Get().Send( caller_name_, msg );     \
      }                                                    \
    };                                                     \

    GENERATE_NONERROR_STREAMSTRUCT(Warning)
    GENERATE_NONERROR_STREAMSTRUCT(Info)
    GENERATE_NONERROR_STREAMSTRUCT(Debug)
    GENERATE_NONERROR_STREAMSTRUCT(Trace)

#undef GENERATE_NONERROR_STREAMSTRUCT

    static Error error_;
    static Warning warning_;
    static Info info_;
    static Debug debug_;
    static Trace trace_;
    
    static std::string caller_name_;
  };

  std::string DAQLogger::caller_name_ = "";

}

#endif /* lbne_artdaq_DAQLogger_DAQLogger_hh */
