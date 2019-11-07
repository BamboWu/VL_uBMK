#include <stdint.h>
#include "timing.h"

/*
 * Read Time Stamp Counter (TSC)
 */
__inline__ uint64_t rdtsc() {
#ifdef __x86_64__
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo) | ( ((uint64_t)hi) << 32);
#elif __ARM_ARCH == 8
  uint64_t cntvct;
  __asm__ __volatile__ ("mrs %0, CNTVCT_EL0" : "=r"(cntvct));
  return cntvct;
#else
  return 0;
#endif
}
