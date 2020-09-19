#ifndef _TIMING_H__
#define _TIMING_H__  1
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read Time Stamp Counter (TSC)
 */
__inline__ uint64_t rdtsc() __attribute__((always_inline));
uint64_t rdtsc() {
#ifdef __x86_64__
  uint64_t val = 0;
  __asm__ volatile(
#if HAS_RDTSCP
          "rdtscp                            \n\r"
#else
          "lfence                            \n\r"
          "rdtsc                             \n\r"
#endif
          "\
            shl      $32, %%rdx              \n\
            orq      %%rax, %%rdx            \n\
            movq     %%rdx, %[val]               "
            :
            /*outputs here*/
            [val]    "=r" (val)
            :
            /*inputs here*/
            :
            /*clobbered registers*/
            "rax","eax","rcx","ecx","rdx"
            );
  return val;
#elif __ARM_ARCH == 8
  uint64_t cntvct;
  __asm__ __volatile__ (
          "dsb SY                            \n\r"
          "mrs %0, CNTVCT_EL0                \n\r"
          : "=r"(cntvct));
  return cntvct;
#else
#warning "using clock_gettime"
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  double time_taken;
  time_taken = (t.tv_sec * 1e9);
  time_taken = (time_taken + t.tv_nsec);
  return time_taken; /** nanoseconds **/
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* END _TIMING_H__ */
