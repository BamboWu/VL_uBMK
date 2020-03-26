#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

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


#define NOSCE 1
#undef  NOSCE 

struct
{
    char  r;
    char  padd[ 63 ];
} volatile lock = { .r = 'R' };

uint8_t *arr;

uint8_t __attribute__((aligned(64))) cl[64];


uint8_t *max;

void *producer(void *args) {
  setAffinity(0);
  int round;
  for (round = 0; MAX_ROUND > round; ++round) {
    while ('P' != lock.r ){ __asm__ volatile( "nop" ::: ); }
    uint8_t offset;
    for( offset = 0; MAX_LEN > offset; ++offset ) 
    {
       cl[offset] = arr[ round * MAX_LEN + offset ];
    }
    
#ifdef NOSCE
    __asm__ volatile ( "nop" : : : );
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
    lock.r = 'C';
  }
  return NULL;
}

void *consumer(void *args) {
  setAffinity(1);
  int round;
  for( round = 0; MAX_ROUND > round; ++round) 
  {
    while( 'C' != lock.r ){ __asm__ volatile( "nop" ::: ); }
    uint8_t tmp_max = 0;
    uint8_t offset;
    for (offset = 0; MAX_LEN > offset; ++offset) 
    {
      if (cl[offset] > tmp_max) 
      {
        tmp_max = cl[offset];
      }
    }
    max[ round ] = tmp_max;
    lock.r = 'P';
  }
  return NULL;
}

int main(int argc, char *argv[]) 
{

#if SETSCHED
int priority = sched_get_priority_max( SCHED_RR );
struct sched_param sp = { .sched_priority = priority };
(void) /** ignore ret val **/
sched_setscheduler(0x0 /* this */,
                   SCHED_RR,
                   &sp);

#endif

  arr = (uint8_t*) malloc( MAX_ROUND * MAX_LEN * sizeof(uint8_t) );
  
  int round;
  for ( round = 0; MAX_ROUND > round; ++round ) 
  {
    uint8_t offset;
    for (offset = 0; MAX_LEN > offset; ++offset) 
    {
        arr[ round * MAX_LEN + offset ] = rand() % (round + 1);
    }
    arr[round * MAX_LEN + (rand() % MAX_LEN)] = round + 1;
  }

  max = (uint8_t*) malloc( MAX_ROUND * sizeof(uint8_t) );
  
  pthread_t threads[ 2 ];
  pthread_create(&threads[ 0 ], NULL, producer, NULL);
  pthread_create(&threads[ 1 ], NULL, consumer, NULL);
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  lock.r = 'P';
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  printf("average ticks: %f\n", (end - beg) / (double) MAX_ROUND );
  return 0;
}
