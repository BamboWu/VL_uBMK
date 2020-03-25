#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#include "affinity.h"
#include "timing.h"

volatile char __attribute__((aligned(64))) r[64] = "M";
volatile uint8_t *arr;
volatile uint8_t __attribute__((aligned(64))) cl[64];
volatile uint8_t max_len = 16;
volatile uint8_t max_round = 64;
uint8_t *max;

void *producer(void *args) {
  setAffinity(0);
  uint8_t round;
  for (round = 0; max_round > round; ++round) {
    while ('P' != r[0]);
    uint8_t offset;
    for (offset = 0; max_len > offset; ++offset) {
      cl[offset] = arr[round * max_len + offset];
    }
#ifdef NOSCE
#elif defined(__x86_64__)
    __asm__ volatile (
        "      clflush (%[cl])    \n\r"
        :
        : [cl]"r" (cl)
        : "memory"
        );
#elif __ARM_ARCH == 8
    __asm__ volatile (
        "      dc cvac, %[cl]     \n\r"
        :
        : [cl]"r" (cl)
        : "memory"
        );
#endif
    r[0] = 'C';
  }
  return NULL;
}

void *consumer(void *args) {
  setAffinity(1);
  uint8_t round;
  for (round = 0; max_round > round; ++round) {
    while ('C' != r[0]);
    uint8_t tmp_max = 0;
    uint8_t offset;
    for (offset = 0; max_len > offset; ++offset) {
      if (cl[offset] > tmp_max) {
        tmp_max = cl[offset];
      }
    }
    max[round] = tmp_max;
    r[0] = 'P';
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (1 < argc) {
    max_round = (uint8_t) atoi(argv[1]);
    if (2 < argc) {
      max_len = (uint8_t) atoi(argv[2]);
    }
  }

  arr = (uint8_t*) malloc(max_round * max_len * sizeof(uint8_t));
  uint8_t round;
  for (round = 0; max_round > round; ++round) {
    uint8_t offset;
    for (offset = 0; max_len > offset; ++offset) {
      arr[round * max_len + offset] = rand() % (round + 1);
    }
    arr[round * max_len + (rand() % max_len)] = round + 1;
  }

  max = (uint8_t*) malloc(max_round * sizeof(uint8_t));
  pthread_t threads[2];
  pthread_create(&threads[0], NULL, producer, NULL);
  pthread_create(&threads[1], NULL, consumer, NULL);
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  r[0] = 'P';
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  printf("ticks: %lu\n", end - beg);
  return 0;
}
