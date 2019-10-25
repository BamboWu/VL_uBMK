#ifndef _AFFINITY_H__
#define _AFFINITY_H__  1

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#include <pthread.h>
#include "check.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void setAffinity(const int desired_core);
extern void nameThread(const char *desired_name);

#ifdef __cplusplus
}
#endif

#endif /* END _AFFINITY_H__ */
