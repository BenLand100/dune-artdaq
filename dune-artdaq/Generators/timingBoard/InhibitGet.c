#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>

#include "stdint.h"
#include "InhibitGet.h"

#include "trace.h"

struct InhibitGet_vars {
  void *context;
  void *subscriber;
  uint32_t timetoignore;
  struct timeval timestart;
};

static struct InhibitGet_vars gIGhandle;

void InhibitGet_init(uint32_t timetoignore) {
  //  Prepare our context and publisher
  gIGhandle.context = zmq_ctx_new ();
  gIGhandle.subscriber = zmq_socket (gIGhandle.context, ZMQ_SUB);
  
  char filter[] = "INHIBITMSG_";
  const int hwm=1;
  int rc = zmq_setsockopt (gIGhandle.subscriber, ZMQ_SUBSCRIBE,
		       filter, strlen (filter));
  rc = zmq_setsockopt(gIGhandle.subscriber,ZMQ_RCVHWM,&hwm,sizeof(int));

  InhibitGet_retime(timetoignore);
#ifdef DEBUGPRINT
  printf("Everything is set up...status=%d\n",rc);
#else
  if (rc) { }   // Avoid unused variable compiler warning
#endif
}

void InhibitGet_connect(const char* zmq_conn)
{
  int rc = zmq_connect (gIGhandle.subscriber, zmq_conn);
  if(rc!=0){
    printf("zmq_connect failed!\n");
  }
}

void InhibitGet_retime(uint32_t timetoignore) {
  // In case init takes a long time, allow it to restart the clock at start()
  gIGhandle.timetoignore = timetoignore;
  gettimeofday(&(gIGhandle.timestart),NULL);
}

uint32_t InhibitGet_get() {
// Look for the latest Inhibit status, and report: 0=No update, 1=OK, 2=Bad (i.e. inhibit)
  struct timeval timenow;
  char recv_buf[256];

// Wait for up to gIGhandle.timetoignore usecs for the first response.
// After first time through, set gIGhandle.timetoignore to zero
    int msg_size = zmq_recv(gIGhandle.subscriber,recv_buf,256,ZMQ_DONTWAIT);
    if(msg_size>0){
      TRACE(10, recv_buf);
    }
    if (msg_size > 14) {
      gIGhandle.timetoignore = 0;
      if ((recv_buf[12]  == 'O' || recv_buf[12] == 'o') &&
          (recv_buf[13] == 'N' || recv_buf[13] == 'n')){
        TRACE(10, "InhibitGet returning 1 (OK)");
        return 1;  // OK
      }
      TRACE(10, "InhibitGet returning 2 (Bad)");
      return 2;  // Bad
    } else if (msg_size >= 0) {
#ifdef DEBUGPRINT
      printf("InhibitGet_get(): error.  Short message received %d %s\n",msg_size,recv_buf);
#endif
      return 0;   // Indicates no update, keep state as it was before
    }

    if (gIGhandle.timetoignore == 0) return 0; // No update, keep state as it was before

    if (gIGhandle.timetoignore < 10000000) {  // We treat a setting of timetoignore >10secs as infinite
      gettimeofday(&timenow,NULL);
      uint64_t tnow = (uint64_t) timenow.tv_sec*1000000 + (uint64_t) timenow.tv_usec;
      uint64_t tthen = (uint64_t) (gIGhandle.timestart).tv_sec*1000000 + 
                       (uint64_t) (gIGhandle.timestart).tv_usec;
#ifdef DEBUGPRINT
      printf("tnow = 0x%lx tthen = 0x%lx diff = %ld\n",tnow,tthen,tnow-tthen);
#endif

      if (tnow-tthen > gIGhandle.timetoignore) {
        TRACE(1, "No response from inhibit master after %d us. Allowing triggers\n",
               gIGhandle.timetoignore);
        gIGhandle.timetoignore = 0;
        return 1;   // Give up on connecting to InhibitMaster and start sending triggers
      }
    }
    usleep(10000);   // 10 ms
    return 0;   // Not connected to InhibitMaster yet, keep no change

}
// printf("%s received at %ld seconds and %u microseconds\n",recv_buf,timenow.tv_sec,timenow.tv_usec);    


void InhibitGet_end() {
  zmq_close(gIGhandle.subscriber);
  zmq_ctx_destroy (gIGhandle.context);
  return;
}

#ifdef WANT_MAIN
int main(){
  int i;
  int timeToIgnore = 50000000;   // Units of microsecs

  InhibitGet_init("tcp://pddaq-gen05-daq0:5566", timeToIgnore);

  for (i=0;i<100;i++) {
    uint32_t g = InhibitGet_get();
    printf("InhibitGet_get returns %d\n",g);
    usleep(300000);   // 300ms
  }

  InhibitGet_end();
}
#endif // WANT_MAIN
