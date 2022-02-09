#define _MULTI_THREADED
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include "check.h"

#define BUFFER_LENGTH 1000

#define CORE_LIST_LEN 128

static int core_list[CORE_LIST_LEN];
static int core_list_max = 0;

/*
 * Parse a string to populate the core_list
 */
int parseCoreList(const char *core_list_str) {
    int list_idx = 0;
    int char_idx = 0;
    while (CORE_LIST_LEN > list_idx) {
        if (',' == core_list_str[char_idx] || /* seperated by comma */
            '\0' == core_list_str[char_idx]) {
            int val1 = 0;
            int val0 = 0;
            int idx_tmp = char_idx - 1;
            int exp = 1;
            while (0 <= idx_tmp && '0' <= core_list_str[idx_tmp] &&
                    core_list_str[idx_tmp] <= '9') {
                val1 += (core_list_str[idx_tmp] - '0') * exp;
                exp *= 10;
                idx_tmp--;
            }
            if (0 > idx_tmp || ',' == core_list_str[idx_tmp]) {
                core_list[list_idx++] = val1;
            } else { /* assert('-' == core_list_str[idx_tmp]) */
                idx_tmp--;
                exp = 1;
                while (0 <= idx_tmp && '0' <= core_list_str[idx_tmp] &&
                        core_list_str[idx_tmp] <= '9') {
                    val0 += (core_list_str[idx_tmp] - '0') * exp;
                    exp *= 10;
                    idx_tmp--;
                }
                /* assert(0 > idx_tmp || ',' == core_list_str[idx_tmp]) */
                /* assert(val0 < val1) */
                while (val0 <= val1) {
                    core_list[list_idx++] = val0++;
                    if (CORE_LIST_LEN <= list_idx) {
                        break;
                    }
                }
            }
        }
        if ('\0' == core_list_str[char_idx]) { /* reach the end of the str */
            break;
        } else {
            char_idx++;
        }
    }
    core_list_max = list_idx;
    return list_idx;
}

/* Forward declaration */
void setAffinity(const int desired_core);
/*
 * Pin a thread to the core specified in the core list at a certain index
 */
void pinAtCoreFromList(const int idx) {
    setAffinity(core_list[idx % core_list_max]);
}

/*
 * Bind a thread to the specified core.
*/
void setAffinity(const int desired_core) {
  cpu_set_t *cpuset = (cpu_set_t*) NULL;
  int cpu_allocate_size = -1;
#if (__GLIBC_MINOR__ > 9) && (__GLIBC__ == 2)
  const int processors_to_allocate = 1;
  cpuset = CPU_ALLOC(processors_to_allocate);
  cpu_allocate_size = CPU_ALLOC_SIZE(processors_to_allocate);
  CPU_ZERO_S(cpu_allocate_size, cpuset);
#else
  cpu_allocate_size = sizeof(cpu_set_t);
  cpuset = (cpu_set_t*) malloc(cpu_allocate_size);
  CPU_ZERO(cpuset);
#endif
  CPU_SET(desired_core, cpuset);
  errno = 0;
  if(0 != sched_setaffinity(0 /* calling thread */,
                            cpu_allocate_size, cpuset)) {
    char buffer[BUFFER_LENGTH];
    memset(buffer, '\0', BUFFER_LENGTH);
    const char *str = strerror_r(errno, buffer, BUFFER_LENGTH);
    fprintf(stderr, "Set affinity failed with error message( %s )\n", str);
    exit(EXIT_FAILURE);
  }
  /** wait till we know we're on the right processor **/
  if(0 != sched_yield()) {
    perror("Failed to yield to wait for core change!\n");
  }
}

/*
 * Name a thread.
 */
void nameThread(const char *desired_name) {
  int rc;
  rc = pthread_setname_np(pthread_self(), desired_name);
  checkResults("pthread_setname_np()", rc);
}

/*
 * Get the OS PID
 */
pid_t getPID() {
  return syscall(SYS_gettid);
}

/*
 * Get the number of context switches of a thread
 */
int getContextSwitches(pid_t pid) {
  int nonvoluntary = 0;
  int voluntary = 0;
  char proc_pid[32];
  sprintf(proc_pid, "/proc/%u/status", pid);
  FILE* file = fopen(proc_pid, "r");
  char* buf = NULL;
  size_t len = 0;
  ssize_t read;
  while ((read = getline(&buf, &len, file)) != -1) {
    if (0 == strncmp(buf, "nonvoluntary_ctxt_switches:", 27)) {
      nonvoluntary = atoi(&buf[27]);
    } else if (0 == strncmp(buf, "voluntary_ctxt_switches:", 24)) {
      voluntary = atoi(&buf[24]);
    }
  }
  fclose(file);
  return (nonvoluntary + voluntary);
}
