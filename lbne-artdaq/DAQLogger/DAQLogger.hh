#ifndef lbne_artdaq_DAQLogger_DAQLogger_hh
#define lbne_artdaq_DAQLogger_DAQLogger_hh

#include "lbne-artdaq/RCConnection/RCConnection.hh"

#include "messagefacility/MessageLogger/MessageLogger.h"

#include <stdexcept>
#include <string>
#include <memory>

// JCF, 5/6/15

// The DAQLogger namespace contains a set of classes designed to allow
// users to make calls such as

// DAQLogger::LogError("mymodule") << "My error message";

// ...thereby replicating the feel of the messagefacility log messages, e.g.

// mf::LogError("mymodule") << "My error message";

// For more documentation, please see
// https://cdcvs.fnal.gov/redmine/projects/lbne-artdaq/wiki/Info_on_Using_DAQLogger


namespace lbne {

  namespace DAQLogger {

    // MessagingBase contains the functionality common to all its
    // inheriting LogXXXXX classes -- including storage of a label
    // passed by the caller, as well as the ability to stream objects
    // in a manner similar to the ostream class
    
    class MessagingBase {
    
    public:

      template <class T>
      MessagingBase& operator<<(const T& t )
      {
	*msgstream_ << t;
	return *this;
      }

      MessagingBase& operator<<(std::ostream&(*f)(std::ostream&))
      {
	*msgstream_ << f;
	return *this;
      }

      MessagingBase& operator<<(std::ios_base&(*f)(std::ios_base&))
      {
	*msgstream_ << f;
	return *this;
      }
      
    protected:

      // Declaring the constructor and destructor protected
      // effectively means users can't accidentally employ
      // MessagingBase directly

      MessagingBase(const std::string& caller_name) : 
	caller_name_(caller_name),
	msgstream_(new std::ostringstream)
      {
      }

      // The functionality of the derived classes should all be
      // defined in overrides of this do-nothing destructor
      virtual ~MessagingBase() noexcept(false) {};

      const std::string CallerName() { return caller_name_; }
      const std::string Msg() { return msgstream_->str(); }

    private:

      const std::string caller_name_;
      std::unique_ptr<std::ostringstream> msgstream_;

    };
  
    // JCF, 5/6/15

    // As a guideline, it's usually a bad idea to create a macro --
    // however, in this namespace I'm going to be declaring four
    // structs inheriting from the MessagingBase class, each of which
    // overrides MessagingBase's destructor with special code that
    // executes whenever an instance of the class is destroyed. Three
    // of the four destructors have almost exactly the same form (for
    // LogWarning, LogInfo, and LogDebug), so a lot of potential
    // typo-based errors and hassle can be prevented by this macro

#define GENERATE_NONERROR_LOGSTRUCT(NAME, REPORT)                	\
    struct Log ## NAME : public MessagingBase {		         	\
								        \
      Log ## NAME(const std::string& caller_name) :      		\
	MessagingBase(caller_name)			         	\
      {						         		\
      }							         	\
					            			\
						        		\
      virtual ~Log ## NAME() noexcept(false) {		        	\
							        	\
	mf::Log ## NAME(CallerName()) << Msg();	         		\
							        	\
        if (REPORT) {				        		\
	  RCConnection::Get().Send( CallerName(), Msg(), # NAME );	\
	}						         	\
      }							         	\
};								

GENERATE_NONERROR_LOGSTRUCT(Warning, true)
GENERATE_NONERROR_LOGSTRUCT(Info, false)
GENERATE_NONERROR_LOGSTRUCT(Debug, false)

#undef GENERATE_NONERROR_LOGSTRUCT

// LogError has a sufficiently complex destructor (i.e., one that
// directly throws an intended exception) that it gets its own
// implementation, rather than a macro

    struct LogError : public MessagingBase {

      class ExceptClass;

      // This is the exception class which is automatically thrown on
      // calls to DAQLogger::LogError()

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

      LogError(const std::string& caller_name) : 
	MessagingBase(caller_name)
      {
      }

      virtual ~LogError() noexcept(false) {

	mf::LogError(CallerName()) << Msg();

	RCConnection::Get().Send( CallerName(), Msg(), "Error" );
	
	ExceptClass myexcept( Msg() );
	throw myexcept;
      }
    };

    } // End namespace DAQLogger
} // End namespace lbne

#endif /* lbne_artdaq_DAQLogger_DAQLogger_hh */
