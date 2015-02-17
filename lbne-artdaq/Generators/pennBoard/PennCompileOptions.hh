#ifndef PENNCOMPILEOPTIONS_HH_
#define PENNCOMPILEOPTIONS_HH_

//are we using the Penn emulator?
#define PENN_EMULATOR

//does the millislice contain:
//  N complete uslices
//or
//  reblocked in a set time-width
#define REBLOCK_USLICE

//does we request on the data stream:
//  1) the 4 bytes header
//  2) a 4 bytes payload header
//  3) an N bytes payload
//  4) repeat 2)-3) until the uslice is finished
//or
//  1) the 4 bytes header
//  2) the rest of the uslice in one go
//#define RECV_PENN_USLICE_IN_CHUNKS


//TODO make REBLOCK_USLICE and RECV_PENN_USLICE_IN_CHUNKS work when both switched on


//#define NO_PENN_CLIENT

#endif //PENNCOMPILEOPTIONS_HH_
