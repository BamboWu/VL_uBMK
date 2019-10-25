#include <stdint.h>

/*
 * Read Time Stamp Counter (TSC)
 */
__inline__ uint64_t rdtsc() {
  unsigned hi, lo;
  __asm__ __volatile__ ( "rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo) | ( ((uint64_t)hi) << 32);
}
