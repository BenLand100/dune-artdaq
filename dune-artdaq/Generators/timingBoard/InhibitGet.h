#ifndef INHIBITGET_H
#define INHIBITGET_H

#ifdef __cplusplus
extern "C" {
#endif
// Initialise zmq connection. (Called at fragmentgenerator construction)
void     InhibitGet_init(uint32_t timetoignore);   
// timetoignore (usec) Time to wait before proceeding with enabling triggers anyway

// Restart timer for timetoignore (called at fragmentgenerator start())
void     InhibitGet_retime(uint32_t timetoignore);

// Get status (called multiple times in fragmentgenerator getNext_())
uint32_t InhibitGet_get();   // Returns 0=No update, 1=OK, 2=Not-OK (should throttle)

// End zmq connection (called in fragmentgenerator destructor)
void     InhibitGet_end();

#ifdef __cplusplus
}   /* End of extern C */
#endif

#endif   // INHIBITGET_H
