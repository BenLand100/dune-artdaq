#ifndef LOG_H__
#define LOG_H__

#include <iostream>

namespace SSPDAQ{

  class Log{

  public:
    inline static std::ostream& Error(){return *fErrorStream;}
    inline static std::ostream& Warning(){return *fWarningStream;}
    inline static std::ostream& Info(){return *fInfoStream;}
    inline static std::ostream& Debug(){return *fDebugStream;}
    inline static std::ostream& Trace(){return *fTraceStream;}

    inline static void SetErrorStream(std::ostream& str){fErrorStream=&str;}
    inline static void SetWarningStream(std::ostream& str){fWarningStream=&str;}
    inline static void SetInfoStream(std::ostream& str){fInfoStream=&str;}
    inline static void SetDebugStream(std::ostream& str){fDebugStream=&str;}
    inline static void SetTraceStream(std::ostream& str){fTraceStream=&str;}

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
