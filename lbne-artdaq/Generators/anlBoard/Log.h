#ifndef LOG_H__
#define LOG_H__

#include <iostream>

namespace SSPDAQ{

  //SSPDAQ classes will send console output via Log class, so that output
  //can be redirected by calling code.
  class Log{

  public:
    //Various levels of log importance. By default Error and Warning
    //go to cerr, others to cout
    inline static std::ostream& Error(){return *fErrorStream;}
    inline static std::ostream& Warning(){return *fWarningStream;}
    inline static std::ostream& Info(){return *fInfoStream;}
    inline static std::ostream& Debug(){return *fDebugStream;}
    inline static std::ostream& Trace(){return *fTraceStream;}

    //Use these to direct SSPDAQ log output to externally defined streams
    inline static void SetErrorStream(std::ostream& str){fErrorStream=&str;}
    inline static void SetWarningStream(std::ostream& str){fWarningStream=&str;}
    inline static void SetInfoStream(std::ostream& str){fInfoStream=&str;}
    inline static void SetDebugStream(std::ostream& str){fDebugStream=&str;}
    inline static void SetTraceStream(std::ostream& str){fTraceStream=&str;}

    //Point a stream here to throw the output away
    static std::ostream* junk;
    
  private:
    static std::ostream* fErrorStream;
    static std::ostream* fWarningStream;
    static std::ostream* fInfoStream;
    static std::ostream* fDebugStream;
    static std::ostream* fTraceStream;

  };

}//namespace
#endif
