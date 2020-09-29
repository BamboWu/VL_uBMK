#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>
#include <zmq.h>
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

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

std::atomic<int> ready;

union {
  bool done; // to tell other threads we are done, only master thread writes
  char pad[64];
} volatile __attribute__((aligned(64))) lock = { .done = false };

void *ctx;

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

  // setup zmq sockets
  void *to_slave_cons = zmq_socket(ctx, ZMQ_PULL);
  assert(to_slave_cons);
  assert(0 == zmq_connect(to_slave_cons, "inproc://to_slave"));
  void *to_master_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_master_prod);
  assert(0 == zmq_connect(to_master_prod, "inproc://to_master"));

  Message<int> msg;
  const size_t msg_size = sizeof(msg);
  bool done = false;
  ready++;
  while ((1 + NUM_SLAVES) != ready) { /** spin **/ };
  while (!done) {
    if(0 < zmq_recv(to_slave_cons, &msg, msg_size, ZMQ_DONTWAIT)) {
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
      if (msg.arr.torswap) {
        rswap(arr_tmp, len_tmp);
      } else {
        swap(arr_tmp, len_tmp);
      }
      while (2 < len_tmp) {
        len_tmp >>= 1;
        for (uint64_t beg_tmp = msg.arr.beg; beg_tmp < msg.arr.end;
             beg_tmp += len_tmp) {
          swap(&msg.arr.base[beg_tmp], len_tmp);
        }
      }
      assert(msg_size == zmq_send(to_master_prod, &msg, msg_size, 0));
    } else {
      if (EAGAIN == errno) {
      }
    }
    done = lock.done;
  }

  assert(0 == zmq_close(to_slave_cons));
  assert(0 == zmq_close(to_master_prod));
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);

  // setup zmq sockets
  ctx = zmq_ctx_new();
  assert(ctx);
  void *to_slave_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_slave_prod);
  const int bufl = (MAX_ON_THE_FLY << 1) * sizeof(int);
  const int zero = 0;
  assert(0 == zmq_setsockopt(to_slave_prod, ZMQ_SNDBUF, &bufl, sizeof(bufl)));
  assert(0 == zmq_setsockopt(to_slave_prod, ZMQ_SNDHWM, &zero, sizeof(zero)));
  assert(0 == zmq_bind(to_slave_prod, "inproc://to_slave"));
  void *to_master_cons = zmq_socket(ctx, ZMQ_PULL);
  assert(to_master_cons);
  assert(0 == zmq_setsockopt(to_master_cons, ZMQ_RCVBUF, &bufl, sizeof(bufl)));
  assert(0 == zmq_setsockopt(to_master_cons, ZMQ_RCVHWM, &zero, sizeof(zero)));
  assert(0 == zmq_bind(to_master_cons, "inproc://to_master"));

  int core_id = 1;
  std::vector<thread> slave_threads;
  ready = 0;
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
  const int msg_size = sizeof(msg);
  uint64_t feed_in = 0;  // record how long the array has been feed in
  uint64_t on_the_fly = 0;  // count how mange messages on the fly
  for (; len > feed_in;) {
    msg.arr.beg = feed_in;
    feed_in += 2;
    msg.arr.end = feed_in;
    assert(msg_size == zmq_send(to_slave_prod, &msg, msg_size, 0));
    if (++on_the_fly >= MAX_ON_THE_FLY) {
      break;
    }
  }

  uint8_t *pcount = new uint8_t[len](); // count number of pairing
  while (true) {
    if (0 < zmq_recv(to_master_cons, &msg, msg_size, ZMQ_DONTWAIT)) {
      const uint64_t beg_tmp = msg.arr.beg;
      const uint64_t end_tmp = msg.arr.end;
      const uint64_t len_sorted = end_tmp - beg_tmp;
      if (len_sorted == len) {
        break; // we are done
      }
      pcount[beg_tmp]++;
      const uint64_t idx_1st = beg_tmp & ~((len_sorted << 1) - 1);
      const uint64_t idx_2nd = idx_1st + len_sorted;
      if (pcount[idx_1st] == pcount[idx_2nd]) {
        msg.arr.beg = idx_1st;
        msg.arr.end = idx_2nd + len_sorted;
        msg.arr.torswap = true;
        assert(msg_size == zmq_send(to_slave_prod, &msg, msg_size, 0));
      } else {
        on_the_fly--;
      }
    } // if (!to_master.empty())
    // feed in remaining array if space
    if (len > feed_in && MAX_ON_THE_FLY > on_the_fly) {
      msg.arr.beg = feed_in;
      feed_in += 2;
      msg.arr.end = feed_in;
      msg.arr.torswap = false;
      assert(msg_size == zmq_send(to_slave_prod, &msg, msg_size, 0));
      on_the_fly++;
    }
  } // while (true)
  //delete[] ccount;
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

  assert(0 == zmq_close(to_slave_prod));
  assert(0 == zmq_close(to_master_cons));
  assert(0 == zmq_ctx_term(ctx));
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
