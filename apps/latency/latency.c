#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "timing.h"
#include "check.h"
//#include "profiling.h" // using function calls has too much overhead for this
#ifndef NOPAPI
#include "papi.h"
#endif

#ifndef ARR_SIZE
#define ARR_SIZE 16384
#endif

#ifndef L1_SIZE
#define L1_SIZE 32768
#endif

#ifndef L2_SIZE
#define L2_SIZE 1048576
#endif

#if __ARM_ARCH == 8
uint64_t CCSIDR2Size(uint32_t ccsidr) {
  const uint64_t line_size = (0x02 == (ccsidr & 0x07)) ? 64 : 0;
  const uint64_t num_ways = 1 << ((ccsidr >> 3) & 0x03FF);
  const uint64_t num_sets = 1 << ((ccsidr >> 13) & 0x7FFF);
  return (line_size * num_ways * num_sets);
}

uint64_t getL1DSize() {
  uint32_t l1d;
  __asm__ volatile (
      "mov x8,          0x0000    \n\r"
      "msr CSSELR_EL1,  x8        \n\r" // EL1 requires to be in kernel space
      "mrs %[l1d],  CCSIDR_EL1    \n\r" // EL1 requires to be in kernel space
      : [l1d]"=r" (l1d)
      :
      : "x8"
      );
  return CCSIDR2Size(l1d);
}

uint64_t getL2Size() {
  uint32_t l2;
  __asm__ volatile (
      "mov x8,          0x0002    \n\r"
      "msr CSSELR_EL1,  x8        \n\r" // EL1 requires to be in kernel space
      "mrs %[l2],   CCSIDR_EL1    \n\r" // EL1 requires to be in kernel space
      : [l2]"=r" (l2)
      :
      : "x8"
      );
  return CCSIDR2Size(l2);
}
#endif // __ARM_ARCH == 8

int main() {
  const uint64_t arr_len = ARR_SIZE / sizeof(uint64_t);
  const uint64_t num_cls = ARR_SIZE / 64;
  uint64_t __attribute__((aligned(64))) arr[arr_len];
  uint64_t pool[num_cls];
  uint64_t idx, cnt;

#ifndef NOPAPI
  static int events = PAPI_NULL;
  static int retval;
  static long long cntvals[6];
  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    printf("Failed with %d at PAPI_library_init()", retval);
    exit(1);
  }
  retval = PAPI_create_eventset(&events);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCR);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCW);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCM);
  if (retval < PAPI_OK) { errorReturn(retval); }

  retval = PAPI_start(events); // rehearse those PAPI function calls once early
  if (retval < PAPI_OK) { errorReturn(retval); }
#endif

  // populate arr in pointer chasing manner
  for (cnt = 0; num_cls > cnt; ++cnt) {
    pool[cnt] = cnt;
  }
  for (cnt = num_cls, idx = 0; 0 < cnt; --cnt) {
    uint64_t pool_idx = rand() % cnt;
    while (0 == pool_idx && 1 < cnt) {
      pool_idx = rand() % cnt; // avoid 0, otherwise chasing ends early
    }
    uint64_t new_idx = pool[pool_idx];
    arr[idx << 3] = (uint64_t)&arr[new_idx << 3]; // x8 $line idx -> arr idx
    idx = new_idx;
    pool[pool_idx] = pool[cnt - 1];
  }

#ifndef NOPAPI
  retval = PAPI_stop(events, cntvals); // rehearsal
  if (retval < PAPI_OK) { errorReturn(retval); }
#endif

  // bring the arr to certain level
  const uint64_t l2_len = L2_SIZE / sizeof(uint64_t);
  const uint64_t l1_len = L1_SIZE / sizeof(uint64_t);
  uint64_t __attribute__((aligned(64))) evict_arr[l2_len + l1_len];
  uint64_t beg_end[6];
  uint64_t sums[5];
#ifdef LATMEM
  beg_end[0] = (uint64_t)arr;
  beg_end[1] = (uint64_t)&arr[arr_len];
  beg_end[2] = (uint64_t)&evict_arr[l2_len];
  beg_end[3] = (uint64_t)&evict_arr[l2_len + l1_len];
  beg_end[4] = (uint64_t)&evict_arr[0];
  beg_end[5] = (uint64_t)&evict_arr[l2_len];
#elif defined(LATL2)
  beg_end[0] = (uint64_t)&evict_arr[0];
  beg_end[1] = (uint64_t)&evict_arr[l2_len];
  beg_end[2] = (uint64_t)arr;
  beg_end[3] = (uint64_t)&arr[arr_len];
  beg_end[4] = (uint64_t)&evict_arr[l2_len];
  beg_end[5] = (uint64_t)&evict_arr[l2_len + l1_len];
#else
  beg_end[0] = (uint64_t)&evict_arr[l2_len];
  beg_end[1] = (uint64_t)&evict_arr[l2_len + l1_len];
  beg_end[2] = (uint64_t)&evict_arr[0];
  beg_end[3] = (uint64_t)&evict_arr[l2_len];
  beg_end[4] = (uint64_t)arr;
  beg_end[5] = (uint64_t)&arr[arr_len];
#endif

#if __ARM_ARCH == 8
  __asm__ volatile (
      "       ldr  x8,  [%[beg], 0]    \n\r"
      "       ldr  x9,  [%[beg], 8]    \n\r"
      "       mov  x11, 0              \n\r"
      "BEG0:  cmp  x8,  x9             \n\r"
      "       b.EQ END0                \n\r"
      "       ldr  x10, [x8]           \n\r"
      "       add  x11, x11, x10       \n\r"
      "       add  x8,  x8,  #64       \n\r"
      "       b    BEG0                \n\r"
      "END0:  str  x11, [%[sum], 0]    \n\r"
      "       dsb  SY                  \n\r"
      "       ldr  x8,  [%[beg], 16]   \n\r"
      "       ldr  x9,  [%[beg], 24]   \n\r"
      "       mov  x11, 0              \n\r"
      "BEG1:  cmp  x8,  x9             \n\r"
      "       b.EQ END1                \n\r"
      "       ldr  x10, [x8]           \n\r"
      "       add  x11, x11, x10       \n\r"
      "       add  x8,  x8,  #64       \n\r"
      "       b    BEG1                \n\r"
      "END1:  str  x11, [%[sum], 8]    \n\r"
      "       dsb  SY                  \n\r"
      "       ldr  x8,  [%[beg], 32]   \n\r"
      "       ldr  x9,  [%[beg], 40]   \n\r"
      "       mov  x11, 0              \n\r"
      "BEG2:  cmp  x8,  x9             \n\r"
      "       b.EQ END2                \n\r"
      "       ldr  x10, [x8]           \n\r"
      "       str  x10, [x8]           \n\r"
      "       add  x8,  x8,  #64       \n\r"
      "       b    BEG2                \n\r"
      "END2:  str  x10, [%[sum], 16]   \n\r"
      "       dsb  SY                  \n\r"
#ifdef CIVAC
      "       ldr  x8,  [%[beg], 32]   \n\r"
      "       ldr  x9,  [%[beg], 40]   \n\r"
      "BEGDC: cmp  x8,  x9             \n\r"
      "       b.EQ ENDDC               \n\r"
      "       dc   civac, x8           \n\r"
      "       add  x8,  x8,  #64       \n\r"
      "       b    BEGDC               \n\r"
      "ENDDC: dsb  SY                  \n\r"
#endif
      "       ldr  x8,  [%[sum], 0]    \n\r"
      "       ldr  x9,  [%[sum], 8]    \n\r"
      "       ldr  x10, [%[sum], 16]   \n\r"
      "       sub  x11, x9,  x8        \n\r"
      "       str  x11, [%[sum], 24]   \n\r"
      "       sub  x11, x10, x8        \n\r"
      "       str  x11, [%[sum], 32]   \n\r"
      :
      : [beg]"r" (beg_end), [sum]"r" (sums)
      : "x8", "x9", "x10", "x11", "cc", "memory"
      );
#elif defined(__x86_64__)
  __asm__ volatile (
      "       movq 0(%[beg]),    %%rsi \n\r"
      "       movq 8(%[beg]),    %%rcx \n\r"
      "       movq $0,           %%rax \n\r"
      "BEG0:  cmp  %%rsi,        %%rcx \n\r"
      "       je   END0                \n\r"
      "       mov  (%%rsi),      %%rdx \n\r"
      "       add  %%rdx,        %%rax \n\r"
      "       add  $64,          %%rsi \n\r"
      "       jmp  BEG0                \n\r"
      "END0:  mov  %%rax,    0(%[sum]) \n\r"
      "       lfence                   \n\r"
      "       movq 16(%[beg]),   %%rsi \n\r"
      "       movq 24(%[beg]),   %%rcx \n\r"
      "       movq $0,           %%rax \n\r"
      "BEG1:  cmp  %%rsi,        %%rcx \n\r"
      "       je   END1                \n\r"
      "       mov  (%%rsi),      %%rdx \n\r"
      "       add  %%rdx,        %%rax \n\r"
      "       add  $64,          %%rsi \n\r"
      "       jmp  BEG1                \n\r"
      "END1:  mov  %%rax,    8(%[sum]) \n\r"
      "       lfence                   \n\r"
      "       movq 32(%[beg]),   %%rsi \n\r"
      "       movq 40(%[beg]),   %%rcx \n\r"
      "       movq $0,           %%rax \n\r"
      "BEG2:  cmp  %%rsi,        %%rcx \n\r"
      "       je   END2                \n\r"
      "       mov  (%%rsi),      %%rdx \n\r"
      "       add  %%rdx,        %%rax \n\r"
      "       add  $64,          %%rsi \n\r"
      "       jmp  BEG2                \n\r"
      "END2:  mov  %%rax,   16(%[sum]) \n\r"
      "       lfence                   \n\r"
#ifdef CLFLUSH
      "       movq 32(%[beg]),   %%rsi \n\r"
      "       movq 40(%[beg]),   %%rcx \n\r"
      "       movq $0,           %%rax \n\r"
      "BEGDC: cmp  %%rsi,        %%rcx \n\r"
      "       je   ENDDC               \n\r"
      "       clflush (%%rsi)          \n\r"
      "       add  $64,          %%rsi \n\r"
      "       jmp  BEGDC               \n\r"
      "ENDDC: lfence                   \n\r"
#endif
      "       movq 0(%[sum]),    %%rdx \n\r"
      "       movq 8(%[sum]),    %%rax \n\r"
      "       sub  %%rdx,        %%rax \n\r"
      "       movq %%rax,   24(%[sum]) \n\r"
      "       movq 8(%[sum]),    %%rdx \n\r"
      "       movq 16(%[sum]),   %%rax \n\r"
      "       sub  %%rdx,        %%rax \n\r"
      "       movq %%rax,   32(%[sum]) \n\r"
      :
      : [beg]"r" (beg_end), [sum]"r" (sums)
      : "rax", "rdx", "rcx", "rsi", "cc", "memory"
      );
#endif

  // timing arr accesses
  uint64_t tsc, frq;
  //const uint64_t beg = rdtsc();
#ifndef NOPAPI
  retval = PAPI_start(events);
  if (retval < PAPI_OK) { errorReturn(retval); }
#endif
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
#if __ARM_ARCH == 8
  __asm__ volatile (
      "       mov  x8,  %[beg]        \n\r"
      "       mov  x9,  x8            \n\r"
      "       dsb  SY                 \n\r"
      "       mrs  x10, CNTVCT_EL0    \n\r"
      "LOOP:  ldr  x8,  [x8]          \n\r"
      "       cmp  x8,  x9            \n\r"
      "       b.NE LOOP               \n\r"
      "       dsb  SY                 \n\r"
      "       mrs  x8, CNTVCT_EL0     \n\r"
      "       sub  %[tsc], x8,  x10   \n\r"
      "       mrs  %[frq], CNTFRQ_EL0 \n\r"
      : [tsc]"=r" (tsc), [frq]"=r" (frq)
      : [beg]"r" (arr)
      : "x8", "x9", "cc"
      );
#elif defined(__x86_64__)
  __asm__ volatile (
      "       movq %[beg],   %%rcx    \n\r"
      "       movq %%rcx,    %%rsi    \n\r"
#if HAS_RDTSCP
      "       rdtscp                  \n\r"
#else
      "       lfence                  \n\r"
      "       rdtsc                   \n\r"
#endif
      "       shl  $32,      %%rdx    \n\r"
      "       orq  %%rax,    %%rdx    \n\r"
      "LOOP:  movq (%%rsi),  %%rsi    \n\r"
      "       cmp  %%rsi,    %%rcx    \n\r"
      "       jne  LOOP               \n\r"
      "       movq %%rdx,    %[tsc]   \n\r"
#if HAS_RDTSCP
      "       rdtscp                  \n\r"
#else
      "       lfence                  \n\r"
      "       rdtsc                   \n\r"
#endif
      "       shl  $32,      %%rdx    \n\r"
      "       orq  %%rax,    %%rdx    \n\r"
      "       sub  %[tsc],   %%rdx    \n\r"
      "       movq %%rdx,    %[tsc]   \n\r"
      : [tsc]"=r" (tsc)
      : [beg]"r" (arr)
      : "rcx", "rsi", "rdx", "rax", "cc"
      );
  const uint64_t frq_beg = rdtsc();
  system("sleep 1");
  frq = (rdtsc() - frq_beg) / 1000000 * 1000000; // round up MHz
#endif
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
#ifndef NOPAPI
  retval = PAPI_stop(events, cntvals);
  if (retval < PAPI_OK) { errorReturn(retval); }
  printf("L2D: reads = %lld, writes = %lld, misses = %lld\n",
         cntvals[0], cntvals[1], cntvals[2]);
#endif
  //const uint64_t end = rdtsc();
  //printf("latency (rdtsc() ticks) = %lu\n", end - beg);
  printf("latency (ns) = %lu * 1e9 / %lu / %lu = %lf\n",
         tsc, frq, num_cls, (tsc * 1000.0 / (frq / 1000000.0) / num_cls));
  return 0;
}