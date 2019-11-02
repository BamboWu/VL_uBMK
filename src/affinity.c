#define _MULTI_THREADED
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include "check.h"

/*
 * Bind a thread to the specified core.
*/
void setAffinity(const int desired_core) {
   int rc;
   cpu_set_t cpuset;
   pthread_t thread = pthread_self();

   CPU_ZERO(&cpuset);
   CPU_SET(desired_core, &cpuset);

   rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
   checkResults("pthread_setaffinity_np()", rc);
}

/*
 * Name a thread.
 */
void nameThread(const char *desired_name) {
   int rc;
   rc = pthread_setname_np(pthread_self(), desired_name);
   checkResults("pthread_setname_np()", rc);
}
