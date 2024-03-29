#ifndef _THREADING_H__
#define _THREADING_H__  1

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#include <pthread.h>
#include <sys/types.h>
#include "check.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int parseCoreList(const char *core_list_str);
extern void threadCreate(pthread_t* hdl, pthread_attr_t* attr,
        void* (*func)(void*), void* args, int idx);
extern void pinAtCoreFromList(const int idx);

extern void setAffinity(const int desired_core);
extern void nameThread(const char *desired_name);

extern pid_t getPID();
extern int getContextSwitches(pid_t pid);

#ifdef __cplusplus
}
#endif

#endif /* END _THREADING_H__ */
