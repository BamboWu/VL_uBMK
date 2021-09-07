#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#include <atomic>

#include <vl/vl.h>

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#include "threading.h"
#include "timing.h"
#include "printmap.h"

/* From libvl/kernel/faked_sysvl.h */
#define VL_MAX 256
/* From libvl/libvl/libvl.h */
struct StructPageNode_s { /* free cachelines what are continuous */
    void *base;
    uint64_t num_inactive; /* to quickly tell #cachelines free to allocate */
    struct StructPageNode_s *next;
};
typedef struct StructPageNode_s StructPageNode;
typedef struct StructPageNode_s *StructPageNodePtr;
struct VirtualLink {
    StructPageNodePtr prod_head_node;
    StructPageNodePtr cons_head_node;
    StructPageNodePtr free_upon_rmvl; /* to avoid ABA */
    volatile void *prod_devmem; /* points to current non-cachable array */
    volatile void *cons_devmem; /* points to current non-cachable array */
};
/* Instantiated in libvl/libvl/mkvl.c */
extern struct VirtualLink links_g[VL_MAX];

int fd;
volatile int prod_core, cons_core;
int nrounds;
std::atomic< int > ready;

int prod_context_switches, cons_context_switches;

void *producer(void *args) {
  setAffinity(prod_core);
  pid_t pid = getPID();
  const int nswitches_before = getContextSwitches(pid);
  vlendpt_t send;
  open_byte_vl_as_producer(fd, &send, 1);
  volatile void *cva = send.pcacheline;
  volatile void *devmem = links_g[send.fd].prod_devmem;
  uint8_t *pbyte = (uint8_t*) cva;
  pbyte[62] = 0x3F; // underflow in producer cacheline means full
  ready++;
  while( 3 != ready ){ /** spin **/ };

  __asm__ volatile (
      "      mov  x8,  %[cnt]     \n\r"
      "PUSH: mov  x9,  %[cva]     \n\r"
#ifndef NOGEM5
      "      .word     0xd508b009 \n\r" /* DC.SVAC */
      "      mov  x9,  %[devmem]  \n\r"
      "      .word     0xd508b029 \n\r" /* DC.PSVAC */
#else
      "      mov  x9,  #0         \n\r"
#endif
      "      b.mi      PUSH       \n\r"
      "      subs x8,  x8,  #1    \n\r"
      "      b.ne      PUSH       \n\r"
      :
      : [cnt]"r" (nrounds), [cva]"r" (cva), [devmem]"r" (devmem)
      : "x8", "x9", "cc"
      );

  const int nswitches_after = getContextSwitches(pid);
  prod_context_switches = nswitches_after - nswitches_before;
  return NULL;
}
void *consumer(void *args) {
  setAffinity(cons_core);
  pid_t pid = getPID();
  const int nswitches_before = getContextSwitches(pid);
  vlendpt_t recv;
  open_byte_vl_as_consumer(fd, &recv, 32);
  volatile void *cva = recv.pcacheline;
  volatile void *devmem = links_g[recv.fd].cons_devmem;
  ready++;
  while( 3 != ready ){ /** spin **/ };

  __asm__ volatile (
      "        mov  x8,  %[cnt]     \n\r"
      "        mov  x9,  %[cva]     \n\r"
      "POP:                         \n\r"
#ifndef NOGEM5
      "        .word     0xd508b009 \n\r" /* DC.SVAC */
      "        mov  x10, %[devmem]  \n\r"
      "        .word     0xd508b04a \n\r" /* DC.PSVAC */
#else
      "        mov  w10, 0x01       \n\r"
      "        strb w10, [x9, #62]  \n\r"
#endif
      "        ldrb w10, [x9, #62]  \n\r" /* From DEQB */
      "        and  w10, w10, 0x03F \n\r"
      "        cmp  w10, #61        \n\r"
      "        b.le FILLED          \n\r"
      "        ldrb w10, [x9, #63]  \n\r" /* From VL_Next */
      "        sxtb x10, w10        \n\r"
      "        lsl  x10, x10, #6    \n\r"
      "        add  x9,  x9,  x10   \n\r"
      "        b    POP             \n\r"
      "FILLED: mov  w10, 0x03F      \n\r"
      "        strb w10, [x9, #62]  \n\r"
      "        subs x8,  x8,  #1    \n\r"
      "        b.ne      POP        \n\r"
      :
      : [cnt]"r" (nrounds), [cva]"r" (cva), [devmem]"r" (devmem)
      : "x8", "x9", "x10", "cc", "memory"
      );

  const int nswitches_after = getContextSwitches(pid);
  cons_context_switches = nswitches_after - nswitches_before;
  return NULL;
}

int main(int argc, char *argv[]) {

#ifndef NOSCHEDRR
  int priority = sched_get_priority_max(SCHED_RR);
  struct sched_param sp = { .sched_priority = priority };
  if (0 != sched_setscheduler(0x0 /* this */,
                              SCHED_RR,
                              &sp)) {
    perror("failed to set schedule");
  }
#endif
  setAffinity(0);

  nrounds = 1024;
  prod_core = 8;
  cons_core = 1;
  if (1 < argc) {
    nrounds = atoi(argv[1]);
    if (2 < argc) {
      prod_core = atoi(argv[2]);
      if (3 < argc) {
        cons_core = atoi(argv[3]);
      }
    }
  }

  fd = mkvl();
  ready = 0;
  pthread_t threads[2];
  pthread_create(&threads[0], NULL, producer, NULL);
  pthread_create(&threads[1], NULL, consumer, NULL);
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  ready++;
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  printf("average ticks: %f\n", (end - beg) / (double) nrounds);
  printf("producer context switches: %d\n", prod_context_switches);
  printf("consumer context switches: %d\n", cons_context_switches);
  return 0;
}
