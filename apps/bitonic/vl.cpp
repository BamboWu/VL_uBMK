#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>

#ifndef STDTHREAD
#include <boost/thread.hpp>
#else
#include <thread>
#endif

#include "vl/vl.h"

#include "threading.h"
#include "utils.hpp"

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

int to_rswap_fd;
int to_swap_fd;
int to_connect_fd;

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
  uint64_t arr_base, arr_len, arr_beg, arr_end;
  vlendpt_t to_swap_cons, to_swap_prod, to_conn_prod;
  int errorcode;
  bool isvalid;
  bool done = false;

  // open endpoints
  if ((errorcode = open_double_vl_as_consumer(to_swap_fd, &to_swap_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_swap_cons\n", __func__);
    return;
  }
  if ((errorcode = open_double_vl_as_producer(to_swap_fd, &to_swap_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_swap_prod\n", __func__);
    return;
  }
  if ((errorcode =
       open_double_vl_as_producer(to_connect_fd, &to_conn_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_conn_prod\n", __func__);
    return;
  }

  while (!done) {
    double_vl_pop_non(&to_swap_cons, &arr_base, &isvalid);
    if (isvalid) {
      double_vl_pop_weak(&to_swap_cons, &arr_len);
      double_vl_pop_weak(&to_swap_cons, &arr_beg);
      double_vl_pop_weak(&to_swap_cons, &arr_end);
      int *arr_tmp = (int *)arr_base;
      arr_tmp = &arr_tmp[arr_beg];
      uint64_t len_tmp = arr_end - arr_beg;
      swap(arr_tmp, len_tmp);
      if (2 < len_tmp) {
        len_tmp >>= 1;
        arr_end = arr_beg + len_tmp;
        double_vl_push_weak(&to_swap_prod, arr_base);
        double_vl_push_weak(&to_swap_prod, arr_len);
        double_vl_push_weak(&to_swap_prod, arr_beg);
        double_vl_push_weak(&to_swap_prod, arr_end);
        double_vl_flush(&to_swap_prod);
        arr_beg += len_tmp;
        arr_end += len_tmp;
        double_vl_push_weak(&to_swap_prod, arr_base);
        double_vl_push_weak(&to_swap_prod, arr_len);
        double_vl_push_weak(&to_swap_prod, arr_beg);
        double_vl_push_weak(&to_swap_prod, arr_end);
        double_vl_flush(&to_swap_prod);
      } else {
        double_vl_push_weak(&to_conn_prod, arr_base);
        double_vl_push_weak(&to_conn_prod, arr_len);
        double_vl_push_weak(&to_conn_prod, arr_beg);
        double_vl_push_weak(&to_conn_prod, arr_end);
        double_vl_flush(&to_conn_prod);
      }
    }
    done = lock.done;
  }
}

void rswap_worker(const int desired_core) {
  setAffinity(desired_core);
  uint64_t arr_base, arr_len, arr_beg, arr_end;
  vlendpt_t to_rswap_cons, to_swap_prod;
  int errorcode;
  bool isvalid;
  bool done = false;;

  // open endpoints
  if ((errorcode =
       open_double_vl_as_consumer(to_rswap_fd, &to_rswap_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_rswap_cons\n", __func__);
    return;
  }
  if ((errorcode = open_double_vl_as_producer(to_swap_fd, &to_swap_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_swap_prod\n", __func__);
    return;
  }

  while (!done) {
    double_vl_pop_non(&to_rswap_cons, &arr_base, &isvalid);
    if (isvalid) {
      double_vl_pop_weak(&to_rswap_cons, &arr_len);
      double_vl_pop_weak(&to_rswap_cons, &arr_beg);
      double_vl_pop_weak(&to_rswap_cons, &arr_end);
      int *arr_tmp = (int *)arr_base;
      arr_tmp = &arr_tmp[arr_beg];
      uint64_t len_tmp = arr_end - arr_beg;
      rswap(arr_tmp, len_tmp);
      len_tmp >>= 1;
      arr_end = arr_beg + len_tmp;
      double_vl_push_weak(&to_swap_prod, arr_base);
      double_vl_push_weak(&to_swap_prod, arr_len);
      double_vl_push_weak(&to_swap_prod, arr_beg);
      double_vl_push_weak(&to_swap_prod, arr_end);
      double_vl_flush(&to_swap_prod);
      arr_beg += len_tmp;
      arr_end += len_tmp;
      double_vl_push_weak(&to_swap_prod, arr_base);
      double_vl_push_weak(&to_swap_prod, arr_len);
      double_vl_push_weak(&to_swap_prod, arr_beg);
      double_vl_push_weak(&to_swap_prod, arr_end);
      double_vl_flush(&to_swap_prod);
    }
    done = lock.done;
  }
}

void connector(const int desired_core, const uint64_t len) {
  uint8_t *ccount = new uint8_t[len](); // count number of entering connect()
  uint8_t *pcount = new uint8_t[len](); // count number of entering pair()
  uint64_t arr_base, arr_len, arr_beg, arr_end;
  vlendpt_t to_conn_cons, to_rswap_prod;
  int errorcode;
  bool isvalid;

  // open endpoints
  if ((errorcode =
       open_double_vl_as_consumer(to_connect_fd, &to_conn_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_conn_cons\n", __func__);
    return;
  }
  if ((errorcode =
       open_double_vl_as_producer(to_rswap_fd, &to_rswap_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), to_rswap_prod\n", __func__);
    return;
  }

  while (true) {
    double_vl_pop_non(&to_conn_cons, &arr_base, &isvalid);
    if (isvalid) {
      double_vl_pop_weak(&to_conn_cons, &arr_len);
      double_vl_pop_weak(&to_conn_cons, &arr_beg);
      double_vl_pop_weak(&to_conn_cons, &arr_end);
      const uint64_t beg_tmp = arr_beg;
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
          double_vl_push_weak(&to_rswap_prod, arr_base);
          double_vl_push_weak(&to_rswap_prod, arr_len);
          double_vl_push_weak(&to_rswap_prod, idx_1st);
          double_vl_push_weak(&to_rswap_prod, idx_2nd + len_to_connect);
          double_vl_flush(&to_rswap_prod);
        } // if (paird)
      } // if (connected)
    } // if (isvalid)
  } // while (true)
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  int errorcode;
  vlendpt_t endpt;

  // make vlinks
  to_swap_fd = mkvl();
  if (0 > to_swap_fd) {
    printf("\033[91mFAILED:\033[0m to_swap_fd = mkvl() "
           "return %d\n", to_swap_fd);
    return;
  }
  to_rswap_fd = mkvl();
  if (0 > to_rswap_fd) {
    printf("\033[91mFAILED:\033[0m to_rswap_fd = mkvl() "
           "return %d\n", to_rswap_fd);
    return;
  }
  to_connect_fd = mkvl();
  if (0 > to_connect_fd) {
    printf("\033[91mFAILED:\033[0m to_connect_fd = mkvl() "
           "return %d\n", to_connect_fd);
    return;
  }
  if ((errorcode = open_double_vl_as_producer(to_swap_fd, &endpt, 1))) {
    printf("\033[91mFAILED:\033[0m open_double_vl_as_producer(to_swap_fd) "
           "return %d\n", errorcode);
    return;
  }

  thread connector_thread(connector, core_id++, len);
  std::vector<thread> worker_threads;
  for (int i = 0; NUM_SWAP_WORKERS > i; ++i) {
    worker_threads.push_back(thread(swap_worker, core_id++));
  }
  for (int i = 0; NUM_RSWAP_WORKERS > i; ++i) {
    worker_threads.push_back(thread(rswap_worker, core_id++));
  }

  // every two elements form a biotonic subarray, ready for swap
  for (uint64_t i = 0; len > i; i += 2) {
    double_vl_push_strong(&endpt, (uint64_t)arr);
    double_vl_push_strong(&endpt, len);
    double_vl_push_strong(&endpt, i);
    double_vl_push_strong(&endpt, i + 2);
    double_vl_flush(&endpt);
  }

  connector_thread.join();
  lock.done = true; // tell other worker threads we are done
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
