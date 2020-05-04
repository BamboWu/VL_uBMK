/*
 * This is a microbenchmark to show false sharing (FSh)
 * and motivate Virtual Link (VL).
 *
 * It can be compiled four ways:
 *    gcc -g shopping.c -pthread -lnuma -DATOMIC -DSHARING -o atomicFSh
 *    gcc -g shopping.c -pthread -lnuma -DATOMIC -DPADDING -o atomicPad
 *    gcc -g shopping.c -pthread -lnuma -DDIRECT -DSHARING -o directFSh
 *    gcc -g shopping.c -pthread -lnuma -DDIRECT -DPADDING -o directPad
 *
 * The -DATOMIC macro makes the writes to the lock atomic.
 * The -DPADDING macro avoid cacheline false sharing between read/write threads
 * by add padding in the shared data structure.
 *
 * The usage is:
 *    ./atomicFSh <core> <Name> <Mode> <Budget> [<core> <Name> <Mode> <BDG>]...
 *    ./atomicPad <core> <Name> <Mode> <Budget> [<core> <Name> <Mode> <BDG>]...
 *    ./directFSh <core> <Name> <Mode> <Budget> [<core> <Name> <Mode> <BDG>]...
 *    ./directPad <core> <Name> <Mode> <Budget> [<core> <Name> <Mode> <BDG>]...
 *
 */

#define _MULTI_THREADED
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#ifdef NUMA_AVAILABLE
#include <numa.h>
#endif
#include <sys/types.h>
#include "threading.h"
#include "timing.h"
#include "check.h"

volatile int wait_to_begin = 1;
int max_node_num;
int num_threads;

#define JUST_WATCH -1
#define BUY_MM 0
#define BUY_KITKAT 1
#define BUY_SNICKERS 2
#define BUY_HERSHEYS 3

#define WATCHER 0
#define MM_FAN 1
#define KITKAT_FAN 2
#define SNICKERS_FAN 3
#define HERSHEYS_FAN 4
#define ROUND_ROBIN 5
#define BIASED 6

const int models[][10] = { // models of candy purchase patterns
  {JUST_WATCH, JUST_WATCH, JUST_WATCH, JUST_WATCH, JUST_WATCH,
   JUST_WATCH, JUST_WATCH, JUST_WATCH, JUST_WATCH, JUST_WATCH}, // watcher
  {BUY_MM, BUY_MM, BUY_MM, BUY_MM, BUY_MM,
   BUY_MM, BUY_MM, BUY_MM, BUY_MM, BUY_MM}, // MM fan
  {BUY_KITKAT, BUY_KITKAT, BUY_KITKAT, BUY_KITKAT, BUY_KITKAT,
   BUY_KITKAT, BUY_KITKAT, BUY_KITKAT, BUY_KITKAT, BUY_KITKAT, }, // KitKat fan
  {BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS,
   BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS, BUY_SNICKERS},
  {BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS,
   BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS, BUY_HERSHEYS},
  {JUST_WATCH, BUY_MM, BUY_KITKAT, BUY_SNICKERS, BUY_HERSHEYS,
   JUST_WATCH, BUY_MM, BUY_KITKAT, BUY_SNICKERS, BUY_HERSHEYS}, // Round Robin
  {BUY_MM, BUY_KITKAT, BUY_MM, BUY_SNICKERS, BUY_MM, BUY_KITKAT,
   BUY_MM, BUY_SNICKERS, BUY_MM, BUY_HERSHEYS} // MM=others
};

typedef struct candy_lover_s {
  pthread_t tid; // thread id
  int cid; // core ID
  char *name;
  const int *pattern; // candy purchase pattern
  int64_t budget;
  uint64_t count; // how many candies eventually bought
  uint64_t round; // how many rounds of shopping went through
  uint64_t ticks; // time spent on shopping
} candy_lover_t;

candy_lover_t *candy_lovers;

/*
 * Create a struct where reader fields share a cacheline
 * with the hot write field.
 * Compiling with -DPADDING inserts padding to avoid that sharing.
 */
typedef struct cart_s {
  uint64_t subtotal0;
  uint64_t subtotal1;
  uint64_t reserved;
#if defined(PADDING)
  uint64_t pad[5];  // to make read and write fields on separate cachelines.
#else
  uint64_t pad[1];  // to provoke false sharing.
#endif
  // price fields, for read only
  uint64_t MM;
  uint64_t KitKat;
  uint64_t SNICKERS;
  uint64_t Hersheys;
} cart_t __attribute__((aligned (64)));

cart_t cart;

/*
 * Thread function to simulate the false sharing.
 * The threads will add_and_fetch subtotal field,
 * and read the other fields in the struct.
 */
extern void *shopping_together(void *parm) {

  uint64_t beg, end;
  int64_t budget = ((candy_lover_t *)parm)->budget;
  const int *pattern = ((candy_lover_t *)parm)->pattern;
  const int core = ((candy_lover_t *)parm)->cid;
  const char *name = ((candy_lover_t *)parm)->name;
  int behavior = JUST_WATCH;
  uint64_t price = 0;
  uint64_t count = 0;
  uint64_t round = 0;

  // Pin each thread to a numa node.
  setAffinity(core);
  nameThread(name);

  // Wait for all threads to get created before starting.
  while (wait_to_begin);

  beg = rdtsc();
  for (round = 0;; ++round) {

    if (JUST_WATCH != behavior) {
#if defined(ATOMIC)
      __sync_add_and_fetch(&cart.subtotal1, price);
#else
      cart.subtotal1 += price;
#endif
      count++;
    } else {
      budget -= 1; // decay the budget so that watcher will exit
    }

    behavior = pattern[round % 10];

    switch (behavior) {
      case JUST_WATCH:
        price = *(volatile uint64_t *)&cart.MM;
        break;
      case BUY_MM:
        price = *(volatile uint64_t *)&cart.MM;
        break;
      case BUY_KITKAT:
        price = *(volatile uint64_t *)&cart.KitKat;
        break;
      case BUY_SNICKERS:
        price = *(volatile uint64_t *)&cart.SNICKERS;
        break;
      case BUY_HERSHEYS:
        price = *(volatile uint64_t *)&cart.Hersheys;
    }

    budget -= price;

    if (0 > budget) {
      break;
    }

  }
  end = rdtsc();

  ((candy_lover_t *)parm)->round = round;
  ((candy_lover_t *)parm)->count = count;
  ((candy_lover_t *)parm)->ticks = end - beg;

  pthread_t tid = ((candy_lover_t *)parm)->tid;
  int cpu = sched_getcpu();
#ifdef NUMA_AVAILABLE
  int node = numa_node_of_cpu(cpu);
#else
  int node = 0;
#endif

  printf("Thread %lu done on CPU %d node %d\n", tid, cpu, node);

  return NULL;
}

int main ( int argc, char *argv[] )
{
  int i, rc = 0;

  int num_threads = (argc - 1) / 4;

  if (1 > num_threads) {
    printf("Usage: %s <core> <Name> <Model> <Budget> ", argv[0]);
    printf("[<core> <Name> <Model> <Budget>]...\n");
    exit(-1);
  }

  candy_lovers = malloc(sizeof(candy_lover_t) * num_threads);

  for (i = 0; i < num_threads; ++i) {
    candy_lovers[i].cid = atoi(argv[4 * i + 1]);
    candy_lovers[i].name = argv[4 * i + 2];
    switch (argv[4 * i + 3][0]) {
      case 'w':
      case 'W':
        candy_lovers[i].pattern = models[WATCHER];
        break;
      case 'm':
      case 'M':
        candy_lovers[i].pattern = models[MM_FAN];
        break;
      case 'k':
      case 'K':
        candy_lovers[i].pattern = models[KITKAT_FAN];
        break;
      case 's':
      case 'S':
        candy_lovers[i].pattern = models[SNICKERS_FAN];
        break;
      case 'h':
      case 'H':
        candy_lovers[i].pattern = models[HERSHEYS_FAN];
        break;
      case 'r':
      case 'R':
        candy_lovers[i].pattern = models[ROUND_ROBIN];
        break;
      case 'b':
      case 'B':
        candy_lovers[i].pattern = models[BIASED];
        break;
      default:
        candy_lovers[i].pattern = models[WATCHER];
        break;
    }
    candy_lovers[i].budget = strtoul(argv[4 * i + 4], NULL, 0);
  }

  for (i = 0; i < num_threads; ++i) {
    rc = pthread_create(&candy_lovers[i].tid, NULL, shopping_together,
                        &candy_lovers[i]);
    checkResults("pthread_create()\n", rc);
    usleep(500);
  }

  printf("mem[%p] wait_to_begin\nmem[%p] cart\n", &wait_to_begin, &cart);
  cart.subtotal1 = cart.subtotal0 = 0;
  cart.reserved = 0;
  cart.MM = 2;
  cart.KitKat = 3;
  cart.SNICKERS = 5;
  cart.Hersheys = 7;

  // Sync to let threads start together
  usleep(500);
  wait_to_begin = 0;

  for (i = 0; i < num_threads; ++i) {
     rc = pthread_join(candy_lovers[i].tid, NULL);
     checkResults("pthread_join()\n", rc);
     printf("It took %"PRIu64" ticks ", candy_lovers[i].ticks);
     printf("for %s ", candy_lovers[i].name);
     printf("to go shopping %"PRIu64" rounds ", candy_lovers[i].round);
     printf("to get %"PRIu64" candies\n", candy_lovers[i].count);
  }
  printf("Cart subtotal is %"PRIu64"\n", cart.subtotal1);

  return 0;
}
