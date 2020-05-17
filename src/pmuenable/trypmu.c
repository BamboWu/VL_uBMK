#include <stdint.h>

int main()
{
  uint64_t pmu = 0;
#if __ARM_ARCH == 8
  __asm__ volatile ( "mrs %[pmu],  PMUSERENR_EL0" : [pmu]"=r" (pmu));
#endif
  if (0xd == pmu) {
    return 0;
  } else {
    return -1;
  }
}
