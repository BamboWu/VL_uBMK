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
  Message<int> msg;
  uint64_t arr_beg, arr_end, isrswap;
  vlendpt_t toslave_cons, tomaster_prod;
  int errorcode;
  bool done = false;;
  size_t cnt;

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
    line_vl_pop_non(&toslave_cons, (uint8_t*)&msg, &cnt);
    if (62 == cnt) {
      arr_beg = msg.arr.beg;
      arr_end = msg.arr.end;
      isrswap = msg.arr.torswap;
      int *arr_tmp = msg.arr.base;
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
      line_vl_push_weak(&tomaster_prod, (uint8_t*)&msg, 62);
    }
    done = lock.done;
  }
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  int errorcode;
  uint64_t arr_len, arr_beg, arr_end;
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

  if ((errorcode = open_byte_vl_as_producer(toslave_fd, &toslave_prod, 1))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_producer(toslave_fd) "
           "return %d\n", errorcode);
    return;
  }

  if ((errorcode = open_byte_vl_as_consumer(tomaster_fd, &tomaster_cons, 1))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_consumer(tomaster_fd) "
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
  Message<int> msg(arr, len, 0, 2);
  uint64_t feed_in = 0;  // record how long the array has been feed in
  uint64_t on_the_fly = 0;  // count how mange messages on the fly
  for (; len > feed_in;) {
    msg.arr.beg = feed_in;
    feed_in += 2;
    msg.arr.end = feed_in;
    msg.arr.torswap = false;
    line_vl_push_weak(&toslave_prod, (uint8_t*)&msg, 62);
    if (++on_the_fly > MAX_ON_THE_FLY) {
      break;
    }
  }

  uint8_t *pcount = new uint8_t[len](); // count number of pairing
  size_t cnt;
  while (true) {
    line_vl_pop_non(&tomaster_cons, (uint8_t*)&msg, &cnt);
    if (62 == cnt) {
      arr_len = msg.arr.len;
      arr_beg = msg.arr.beg;
      arr_end = msg.arr.end;
      const uint64_t len_to_connect = arr_end - arr_beg;
      if (len_to_connect == len) {
        break; // we are done
      }
      pcount[arr_beg]++;
      const uint64_t idx_1st = arr_beg & ~((len_to_connect << 1) - 1);
      const uint64_t idx_2nd = idx_1st + len_to_connect;
      if (pcount[idx_1st] == pcount[idx_2nd]) {
        msg.arr.len = arr_len;
        msg.arr.beg = idx_1st;
        msg.arr.end = idx_2nd + len_to_connect;
        msg.arr.torswap = true;
        line_vl_push_weak(&toslave_prod, (uint8_t*)&msg, 62);
      } else {
        on_the_fly--;
      }
    } // if (isvalid)
    // feed in remaining array if space
    if (len > feed_in && MAX_ON_THE_FLY > on_the_fly) {
      msg.arr.len = len;
      msg.arr.beg = feed_in;
      feed_in += 2;
      msg.arr.end = feed_in;
      msg.arr.torswap = true;
      line_vl_push_weak(&toslave_prod, (uint8_t*)&msg, 62);
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
