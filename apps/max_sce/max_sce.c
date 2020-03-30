#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <stdio.h>
#include <errno.h>

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#include "affinity.h"
#include "timing.h"

#ifndef MAX_LEN
#define MAX_LEN     16
#endif

#ifndef MAX_ROUND
#define MAX_ROUND   (1 << 15 )
#endif 


#define NOSCE 1
//#undef  NOSCE 

//#define SETSCHED 1

uint64_t __attribute__((aligned(64)))   foo[ 8 ];

#define PRODUCER 0
#define CONSUMER 1

struct
{
    uint8_t  readyp;
    uint8_t  readyc;
} volatile lock = { .readyp = 0, .readyc = 0 };

#include <assert.h>

int flag;

uint8_t *arr;

uint8_t    __attribute__((aligned(64))) cl[ 64 ];
    


uint8_t *max;

void *producer(void *args) {
  const int other  = CONSUMER; 
  setAffinity( 1 );
  int round = 0;
  lock.readyp = 1;
  /** spin until consumer is ready **/
  while ( lock.readyc != 1 ){ __asm__ volatile( "nop" ::: ); }
  for (; MAX_ROUND > round; ++round) {
    
    int local_flag = -1;
    while( local_flag != PRODUCER )
    {
        __atomic_load ( &flag , &local_flag, __ATOMIC_SEQ_CST );
    }
    //else do this; 
    uint8_t offset;
    for( offset = 0; MAX_LEN > offset; ++offset ) 
    {
       cl[ offset ] = arr[ round * MAX_LEN + offset ];
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
#warning "building with software eviction"
    __asm__ volatile (
        "      dc civac, %[foo]     \n\r"
        //"      dc cvac, %[foo]     \n\r"
        //"      dc civac, %[cl]     \n\r"
        //"      dc cvac, %[cl]     \n\r"
        :
        //: [cl]"r" (cl)
        : [foo]"r" (foo)
        : "memory"
        );
#endif
    
    __atomic_store ( &flag, &other , __ATOMIC_SEQ_CST );
  }
  return NULL;
}

void *consumer(void *args) {
  const int other = PRODUCER;
  setAffinity( 3 );
  int round = 0;
  lock.readyc = 1;
  /** spin until producer is ready **/
  while ( lock.readyp != 1 ){ __asm__ volatile( "nop" ::: ); }

  for( ; MAX_ROUND > round; ++round) 
  {
    int local_flag = -1;
    while( local_flag != CONSUMER )
    {
        __atomic_load ( &flag , &local_flag, __ATOMIC_SEQ_CST );
    }
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
    __atomic_store ( &flag, &other , __ATOMIC_SEQ_CST );
  }
  return NULL;
}

int main(int argc, char *argv[]) 
{
    flag = PRODUCER;
#if SETSCHED
    int priority = sched_get_priority_max( SCHED_RR );
    struct sched_param sp = { .sched_priority = priority };
    if( sched_setscheduler(0x0 /* this */,
                       SCHED_RR,
                       &sp) != 0 )
    {                   
        perror("failed to set schedule" );                    
    }
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
  
  while ( lock.readyp != 1 && lock.readyc != 1 )
  { 
    __asm__ volatile( "nop" ::: ); 
  }
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  pthread_join(threads[0], NULL);
  pthread_join(threads[1], NULL);
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  printf("%f", (end - beg) / (double) MAX_ROUND );
  return 0;
}
