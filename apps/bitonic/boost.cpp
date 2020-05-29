#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>
#include <boost/lockfree/queue.hpp>

#ifndef STDTHREAD
#include <boost/thread.hpp>
#else
#include <thread>
#endif

#include "threading.h"
#include "utils.hpp"

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

boost::lockfree::queue< Message<int> > to_rswap(64);
boost::lockfree::queue< Message<int> > to_swap(64);
boost::lockfree::queue< Message<int> > to_connect(64);

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

void swap_worker(const int desired_core) {
  setAffinity(desired_core);
  Message<int> msg;
  bool done = false;
  while (!done) {
    if (to_swap.pop(msg)) {
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
      swap(arr_tmp, len_tmp);
      if (2 < len_tmp) {
        len_tmp >>= 1;
        msg.arr.end = msg.arr.beg + len_tmp;
        to_swap.push(msg);
        msg.arr.beg += len_tmp;
        msg.arr.end += len_tmp;
        to_swap.push(msg);
      } else {
        to_connect.push(msg);
      }
    }
    done = lock.done;
  }
}

void rswap_worker(const int desired_core) {
  setAffinity(desired_core);
  Message<int> msg;
  bool done = false;;
  while (!done) {
    if (to_rswap.pop(msg)) {
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
      rswap(arr_tmp, len_tmp);
      if (2 < len_tmp) {
        len_tmp >>= 1;
        msg.arr.end = msg.arr.beg + len_tmp;
        to_swap.push(msg);
        msg.arr.beg += len_tmp;
        msg.arr.end += len_tmp;
        to_swap.push(msg);
      } else {
        to_connect.push(msg);
      }
    }
    done = lock.done;
  }
}

void connector(const int desired_core, const uint64_t len) {
  setAffinity(desired_core);
  uint8_t *ccount = new uint8_t[len](); // count number of entering connect()
  uint8_t *pcount = new uint8_t[len](); // count number of entering pair()
  Message<int> msg;
  while (true) {
    if (!to_connect.empty()) {
      to_connect.pop(msg);
      const uint64_t beg_tmp = msg.arr.beg;
      ccount[beg_tmp]++;
      const uint64_t len_to_connect = 1 << ccount[beg_tmp];
      const uint64_t idx_beg = beg_tmp & ~(len_to_connect - 1);
      const uint64_t idx_end = (beg_tmp | (len_to_connect - 1)) + 1;
      bool connected = true;
      for (uint64_t i = idx_beg; idx_end > i; i += 2) {
        if (ccount[i] != ccount[beg_tmp]) {
          connected = false;
          break;
        }
      }
      if (connected) {
        if (len_to_connect == len) {
          delete[] ccount;
          delete[] pcount;
          return; // we are done
        }
        pcount[idx_beg]++;
        const uint64_t idx_1st = idx_beg & ~((len_to_connect << 1) - 1);
        const uint64_t idx_2nd = idx_1st + len_to_connect;
        if (pcount[idx_1st] == pcount[idx_2nd]) {
          msg.arr.beg = idx_1st;
          msg.arr.end = idx_2nd + len_to_connect;
          to_rswap.push(msg);
        } // if (paird)
      } // if (connected)
    } // if (!to_connect.empty())
  } // while (true)
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  thread connector_thread(connector, core_id++, len);
  std::vector<thread> worker_threads;
  for (int i = 0; NUM_SWAP_WORKERS > i; ++i) {
    worker_threads.push_back(thread(swap_worker, core_id++));
  }
  for (int i = 0; NUM_RSWAP_WORKERS > i; ++i) {
    worker_threads.push_back(thread(rswap_worker, core_id++));
  }

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  // every two elements form a biotonic subarray, ready for swap
  Message<int> msg(arr, len, 0, 2);
  for (uint64_t i = 0; len > i; i += 2) {
    msg.arr.beg = i;
    msg.arr.end = i + 2;
    to_swap.push(msg);
  }

  connector_thread.join();
  lock.done = true; // tell other worker threads we are done

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  for (int i = NUM_SWAP_WORKERS + NUM_RSWAP_WORKERS - 1; 0 <= i; --i) {
    worker_threads[i].join();
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
  dump(arr, len);
  std::cout << std::endl;
  check(arr, len);
  free(arr);
  return 0;
}
