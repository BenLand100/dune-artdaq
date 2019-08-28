#ifndef UTILITIES_HH_
#define UTILITIES_HH_

#include <thread>

inline void set_thread_name(std::thread& thread, const char* name, uint32_t tid)
{
    char tname[16];
    snprintf(tname, 16, "flx-%s-%d", name, tid);
    auto handle = thread.native_handle();
    pthread_setname_np(handle, tname);
}

#endif

