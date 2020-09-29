#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>
#include <atomic>

#ifndef STDTHREAD
#include <boost/thread.hpp>
#else
#include <thread>
#endif

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "threading.h"
#include "timing.h"
#include "utils.hpp"

#include "vl/vl.h"

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

int toslave_fd;
int tomaster_fd;
std::atomic<int> ready;

union {
  bool done; // to tell other threads we are done, only master thread writes
  char pad[64];
} volatile __attribute__((aligned(64))) lock = { .done = false };

uint64_t roundup64(const uint64_t val) {
  uint64_t val64 = val - 1;
  val64 |= (val64 >> 1);
  val64 |= (val64 >> 2);
  val64 |= (val64 >> 4);
  val64 |= (val64 >> 8);
  val64 |= (val64 >> 16);
  val64 |= (val64 >> 32);
  return ++val64;
}

void slave(const int desired_core) {
  setAffinity(desired_core);
  uint64_t arr_base, arr_len, arr_beg, arr_end, isrswap, dummy;
  vlendpt_t toslave_cons, tomaster_prod;
  int errorcode;
  bool isvalid;
  bool done = false;;

  // open endpoints
  if ((errorcode = open_twin_vl_as_consumer(toslave_fd, &toslave_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), toslave_cons\n", __func__);
    return;
  }
  if ((errorcode = open_twin_vl_as_producer(tomaster_fd, &tomaster_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), tomaster_prod\n", __func__);
    return;
  }
  ready++;
  while ((1 + NUM_SLAVES) != ready) { /** spin **/ };

  while (!done) {
    twin_vl_pop_non(&toslave_cons, &arr_base, &isvalid);
    if (isvalid) {
      twin_vl_pop_weak(&toslave_cons, &arr_len);
      twin_vl_pop_weak(&toslave_cons, &arr_beg);
      twin_vl_pop_weak(&toslave_cons, &arr_end);
      twin_vl_pop_weak(&toslave_cons, &isrswap);
      twin_vl_pop_weak(&toslave_cons, &dummy);
      twin_vl_pop_weak(&toslave_cons, &dummy);
      int *arr_tmp = (int *)arr_base;
      uint64_t len_tmp = arr_end - arr_beg;
      if (isrswap) {
        rswap(&arr_tmp[arr_beg], len_tmp);
      } else {
        swap(&arr_tmp[arr_beg], len_tmp);
      }
      while (2 < len_tmp) {
        len_tmp >>= 1;
        for (uint64_t beg_tmp = arr_beg; beg_tmp < arr_end;
             beg_tmp += len_tmp) {
          swap(&arr_tmp[beg_tmp], len_tmp);
        }
      }
      twin_vl_push_weak(&tomaster_prod, arr_base);
      twin_vl_push_weak(&tomaster_prod, arr_len);
      twin_vl_push_weak(&tomaster_prod, arr_beg);
      twin_vl_push_weak(&tomaster_prod, arr_end);
      // dummy pushes to let message take a entire cacheline
      twin_vl_push_weak(&tomaster_prod, 0);
      twin_vl_push_weak(&tomaster_prod, 0);
      twin_vl_push_weak(&tomaster_prod, 0);
    }
    done = lock.done;
  }
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  int errorcode;
  bool isvalid;
  uint64_t arr_base, arr_len, arr_beg, arr_end, dummy;
  vlendpt_t toslave_prod, tomaster_cons;

  // make vlinks
  toslave_fd = mkvl();
  if (0 > toslave_fd) {
    printf("\033[91mFAILED:\033[0m toslave_fd = mkvl() "
           "return %d\n", toslave_fd);
    return;
  }
  tomaster_fd = mkvl();
  if (0 > tomaster_fd) {
    printf("\033[91mFAILED:\033[0m tomaster_fd = mkvl() "
           "return %d\n", tomaster_fd);
    return;
  }

  if ((errorcode = open_twin_vl_as_producer(toslave_fd, &toslave_prod, 1))) {
    printf("\033[91mFAILED:\033[0m open_twin_vl_as_producer(toslave_fd) "
           "return %d\n", errorcode);
    return;
  }

  if ((errorcode = open_twin_vl_as_consumer(tomaster_fd, &tomaster_cons, 1))) {
    printf("\033[91mFAILED:\033[0m open_twin_vl_as_consumer(tomaster_fd) "
           "return %d\n", errorcode);
    return;
  }

  ready = 0;
  std::vector<thread> slave_threads;
  for (int i = 0; NUM_SLAVES > i; ++i) {
    slave_threads.push_back(thread(slave, core_id++));
  }
  ready++;
  while ((1 + NUM_SLAVES) != ready) { /** spin **/ };

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  // every two elements form a biotonic subarray, ready for swap
  uint64_t feed_in = 0;  // record how long the array has been feed in
  uint64_t on_the_fly = 0;  // count how mange messages on the fly
  for (; len > feed_in;) {
    twin_vl_push_weak(&toslave_prod, (uint64_t)arr);
    twin_vl_push_weak(&toslave_prod, len);
    twin_vl_push_weak(&toslave_prod, feed_in);
    feed_in += 2;
    twin_vl_push_weak(&toslave_prod, feed_in);
    twin_vl_push_weak(&toslave_prod, 0);  // isrswap = false
    // two dummy pushs to let the message take an entire cacheline
    twin_vl_push_weak(&toslave_prod, 0);
    twin_vl_push_weak(&toslave_prod, 0);
    if (++on_the_fly > MAX_ON_THE_FLY) {
      break;
    }
  }

  uint8_t *pcount = new uint8_t[len](); // count number of pairing
  while (true) {
    twin_vl_pop_non(&tomaster_cons, &arr_base, &isvalid);
    if (isvalid) {
      twin_vl_pop_weak(&tomaster_cons, &arr_len);
      twin_vl_pop_weak(&tomaster_cons, &arr_beg);
      twin_vl_pop_weak(&tomaster_cons, &arr_end);
      twin_vl_pop_weak(&tomaster_cons, &dummy);
      twin_vl_pop_weak(&tomaster_cons, &dummy);
      twin_vl_pop_weak(&tomaster_cons, &dummy);
      const uint64_t len_to_connect = arr_end - arr_beg;
      if (len_to_connect == len) {
        break; // we are done
      }
      pcount[arr_beg]++;
      const uint64_t idx_1st = arr_beg & ~((len_to_connect << 1) - 1);
      const uint64_t idx_2nd = idx_1st + len_to_connect;
      if (pcount[idx_1st] == pcount[idx_2nd]) {
        twin_vl_push_weak(&toslave_prod, arr_base);
        twin_vl_push_weak(&toslave_prod, arr_len);
        twin_vl_push_weak(&toslave_prod, idx_1st);
        twin_vl_push_weak(&toslave_prod, idx_2nd + len_to_connect);
        twin_vl_push_weak(&toslave_prod, 1);  // isrswap = true
        // two dummy pushs to let the message take an entire cacheline
        twin_vl_push_weak(&toslave_prod, 0);
        twin_vl_push_weak(&toslave_prod, 0);
      } else {
        on_the_fly--;
      }
    } // if (isvalid)
    // feed in remaining array if space
    if (len > feed_in && MAX_ON_THE_FLY > on_the_fly) {
      twin_vl_push_weak(&toslave_prod, (uint64_t)arr);
      twin_vl_push_weak(&toslave_prod, len);
      twin_vl_push_weak(&toslave_prod, feed_in);
      feed_in += 2;
      twin_vl_push_weak(&toslave_prod, feed_in);
      twin_vl_push_weak(&toslave_prod, 0);  // isrswap = false
      // two dummy pushs to let the message take an entire cacheline
      twin_vl_push_weak(&toslave_prod, 0);
      twin_vl_push_weak(&toslave_prod, 0);
      on_the_fly++;
    }
  } // while (true)

  lock.done = true; // tell other worker threads we are done
  delete[] pcount;

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  for (int i = NUM_SLAVES - 1; 0 <= i; --i) {
    slave_threads[i].join();
  }
}

int main(int argc, char *argv[]) {
  uint64_t len = 16;
  if (1 < argc) {
    len = strtoull(argv[1], NULL, 0);
  }
  const uint64_t len_roundup = roundup64(len);
  int *arr = (int*) memalign(64, len_roundup * sizeof(int));
  gen(arr, len);
  fill(&arr[len], len_roundup - len, INT_MAX); // padding
  sort(arr, len_roundup);
  //dump(arr, len);
  std::cout << std::endl;
  check(arr, len);
  free(arr);
  return 0;
}
