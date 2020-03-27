#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#include "affinity.h"
#include "timing.h"

#ifndef MAX_LEN
#define MAX_LEN     16
#endif

#ifndef MAX_ROUND
#define MAX_ROUND   (1 << 20)
#endif

uint8_t *arr;

uint8_t __attribute__((aligned(4096))) cls[4096]; // cachelines as circular buf

uint8_t max[MAX_ROUND];

union {
  int round; // to enable pipelining, only producer updates this
  char pad[64];
} volatile __attribute__((aligned(64))) prod = { .round = 0 };

union {
  int round; // to enable pipelining, only consumer updates this
  char pad[64];
} volatile __attribute__((aligned(64))) cons = { .round = -64 };

void *producer(void *args) {
  setAffinity(0);
  int round;
  while(64 <= (prod.round - cons.round)) { __asm__ volatile( "nop" ::: ); }
  for (round = 0; MAX_ROUND > round; ++round) {
    uint8_t *cl = &cls[(round << 6) & 0x0FFF];
    uint64_t offset;
    for (offset = 0; MAX_LEN > offset; ++offset) {
      cl[offset] = arr[round * MAX_LEN + offset];
    }

#ifdef NOSCE
    __asm__ volatile ( "nop" : : : );
#elif defined(__x86_64__)
    __asm__ volatile (
        "      clflush (%[cl])     \n\r"
        :
        : [cl]"r" (cl)
        : "memory"
        );
#elif __ARM_ARCH == 8
    __asm__ volatile (
        "      dc civac, %[cl]     \n\r"
        :
        : [cl]"r" (cl)
        : "memory"
        );
#endif
    prod.round = round + 1;
  }
  return NULL;
}

void *consumer(void *args) {
  setAffinity(1);
  int round;
  for(round = 0; MAX_ROUND > round; ++round) {
    cons.round = round;
    while(round >= prod.round) { __asm__ volatile( "nop" ::: ); }
    uint8_t tmp_max = 0;
    uint8_t offset;
    uint8_t *cl = &cls[(round << 6) & 0x0FFF];
    for (offset = 0; MAX_LEN > offset; ++offset) {
      if (cl[offset] > tmp_max) {
        tmp_max = cl[offset];
      }
    }
    max[round] = tmp_max;
  }
  return NULL;
}

int main(int argc, char *argv[]) {

#if SETSCHED
  int priority = sched_get_priority_max(SCHED_RR);
  struct sched_param sp = { .sched_priority = priority };
  (void) /** ignore ret val **/
  sched_setscheduler(0x0 /* this */,
                     SCHED_RR,
                     &sp);
#endif

  arr = (uint8_t*) malloc(MAX_ROUND * MAX_LEN * sizeof(uint8_t));

  int round;
  for (round = 0; MAX_ROUND > round; ++round) {
    uint8_t offset;
    for (offset = 0; MAX_LEN > offset; ++offset) {
      arr[round * MAX_LEN + offset] = rand() % ((round & 0x00FF) + 1);
    }
    arr[round * MAX_LEN + (rand() % MAX_LEN)] = (round & 0x00FF);
  }

  pthread_t threads[2];
  pthread_create(&threads[0], NULL, producer, NULL);
  pthread_create(&threads[1], NULL, consumer, NULL);
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  cons.round = 0;
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  printf("average ticks: %f\n", (end - beg) / (double) MAX_ROUND);
  for (round = 0; MAX_ROUND > round; ++round) {
    assert((round & 0x00FF) == max[round]);
  }
  return 0;
}
