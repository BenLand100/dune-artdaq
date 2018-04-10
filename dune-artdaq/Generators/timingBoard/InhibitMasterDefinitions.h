#ifndef INHIBITMASTERDEFINITIONS
#define INHIBITMASTERDEFINITIONS 1

#include "zmq.h"
#include "string.h"
#include <sys/time.h>
#include <unistd.h>

const char* STATUS_MSG_MARKER  = "STATUSMSG";
const char* INHIBIT_MSG_MARKER = "INHIBITMSG";

const char* STATUS_BAD  = "BAD";
const char* STATUS_GOOD = "GOOD";

#endif
