#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>
#include <zmq.h>

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

#define MAX_ARRLEN 65536

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

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

void swap_worker(const int desired_core) {
  setAffinity(desired_core);

  const int bufl = (MAX_ARRLEN << 1) * sizeof(int);
  const int zero = 0;
  // setup zmq sockets
  void *to_swap_cons = zmq_socket(ctx, ZMQ_PULL);
  assert(to_swap_cons);
  assert(0 == zmq_setsockopt(to_swap_cons, ZMQ_RCVBUF, &bufl, sizeof(bufl)));
  assert(0 == zmq_setsockopt(to_swap_cons, ZMQ_RCVHWM, &zero, sizeof(zero)));
  assert(0 == zmq_bind(to_swap_cons, "inproc://to_swap"));
  void *to_swap_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_swap_prod);
  assert(0 == zmq_setsockopt(to_swap_prod, ZMQ_SNDBUF, &bufl, sizeof(bufl)));
  assert(0 == zmq_setsockopt(to_swap_prod, ZMQ_SNDHWM, &zero, sizeof(zero)));
  assert(0 == zmq_connect(to_swap_prod, "inproc://to_swap"));
  void *to_connect_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_connect_prod);
  assert(0 == zmq_setsockopt(to_connect_prod, ZMQ_SNDBUF, &bufl, sizeof(int)));
  assert(0 == zmq_setsockopt(to_connect_prod, ZMQ_SNDHWM, &zero, sizeof(int)));
  assert(0 == zmq_connect(to_connect_prod, "inproc://to_connect"));

  Message<int> msg;
  const size_t msg_size = sizeof(msg);
  bool done = false;
  while (!done) {
    if(0 < zmq_recv(to_swap_cons, &msg, msg_size, ZMQ_DONTWAIT)) {
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
      swap(arr_tmp, len_tmp);
      if (2 < len_tmp) {
        len_tmp >>= 1;
        msg.arr.end = msg.arr.beg + len_tmp;
        assert(msg_size == zmq_send(to_swap_prod, &msg, msg_size, 0));
        msg.arr.beg += len_tmp;
        msg.arr.end += len_tmp;
        assert(msg_size == zmq_send(to_swap_prod, &msg, msg_size, 0));
      } else {
        assert(msg_size == zmq_send(to_connect_prod, &msg, msg_size, 0));
      }
    }
    done = lock.done;
  }

  assert(0 == zmq_close(to_swap_cons));
  assert(0 == zmq_close(to_swap_prod));
  assert(0 == zmq_close(to_connect_prod));
}

void rswap_worker(const int desired_core) {
  setAffinity(desired_core);

  // setup zmq sockets
  void *to_rswap_cons = zmq_socket(ctx, ZMQ_PULL);
  assert(to_rswap_cons);
  assert(0 == zmq_bind(to_rswap_cons, "inproc://to_rswap"));
  void *to_swap_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_swap_prod);
  assert(0 == zmq_connect(to_swap_prod, "inproc://to_swap"));
  void *to_connect_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_connect_prod);
  assert(0 == zmq_connect(to_connect_prod, "inproc://to_connect"));

  Message<int> msg;
  const size_t msg_size = sizeof(msg);
  bool done = false;
  while (!done) {
    if(0 < zmq_recv(to_rswap_cons, &msg, msg_size, ZMQ_DONTWAIT)) {
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
      rswap(arr_tmp, len_tmp);
      if (2 < len_tmp) {
        len_tmp >>= 1;
        msg.arr.end = msg.arr.beg + len_tmp;
        assert(msg_size == zmq_send(to_swap_prod, &msg, msg_size, 0));
        msg.arr.beg += len_tmp;
        msg.arr.end += len_tmp;
        assert(msg_size == zmq_send(to_swap_prod, &msg, msg_size, 0));
      } else {
        assert(msg_size == zmq_send(to_connect_prod, &msg, msg_size, 0));
      }
    } else {
      if (EAGAIN == errno) {
      }
    }
    done = lock.done;
  }

  assert(0 == zmq_close(to_rswap_cons));
  assert(0 == zmq_close(to_swap_prod));
  assert(0 == zmq_close(to_connect_prod));
}

void connector(const int desired_core, const uint64_t len) {
  setAffinity(desired_core);
  uint8_t *ccount = new uint8_t[len](); // count number of entering connect()
  uint8_t *pcount = new uint8_t[len](); // count number of entering pair()

  // setup zmq sockets
  void *to_connect_cons = zmq_socket(ctx, ZMQ_PULL);
  assert(to_connect_cons);
  assert(0 == zmq_bind(to_connect_cons, "inproc://to_connect"));
  void *to_rswap_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_rswap_prod);
  assert(0 == zmq_connect(to_rswap_prod, "inproc://to_rswap"));

  Message<int> msg;
  const size_t msg_size = sizeof(msg);
  while (true) {
    if (0 < zmq_recv(to_connect_cons, &msg, msg_size, ZMQ_DONTWAIT)) {
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
          break; // we are done
        }
        pcount[idx_beg]++;
        const uint64_t idx_1st = idx_beg & ~((len_to_connect << 1) - 1);
        const uint64_t idx_2nd = idx_1st + len_to_connect;
        if (pcount[idx_1st] == pcount[idx_2nd]) {
          msg.arr.beg = idx_1st;
          msg.arr.end = idx_2nd + len_to_connect;
          assert(msg_size == zmq_send(to_rswap_prod, &msg, msg_size, 0));
        } // if (paird)
      } // if (connected)
    } // if (!to_connect.empty())
  } // while (true)

  assert(0 == zmq_close(to_connect_cons));
  assert(0 == zmq_close(to_rswap_prod));
  delete[] ccount;
  delete[] pcount;
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);

  // setup zmq sockets
  ctx = zmq_ctx_new();
  assert(ctx);
  void *to_swap_prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(to_swap_prod);
  const int bufl = (MAX_ARRLEN << 1) * sizeof(int);
  const int zero = 0;
  assert(0 == zmq_setsockopt(to_swap_prod, ZMQ_SNDBUF, &bufl, sizeof(bufl)));
  assert(0 == zmq_setsockopt(to_swap_prod, ZMQ_SNDHWM, &zero, sizeof(zero)));
  assert(0 == zmq_connect(to_swap_prod, "inproc://to_swap"));

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
  const int msg_size = sizeof(msg);
  for (uint64_t i = 0; len > i; i += 2) {
    msg.arr.beg = i;
    msg.arr.end = i + 2;
    assert(msg_size == zmq_send(to_swap_prod, &msg, msg_size, 0));
  }

  connector_thread.join();
  lock.done = true; // tell other worker threads we are done

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  for (int i = NUM_SWAP_WORKERS + NUM_RSWAP_WORKERS - 1; 0 <= i; --i) {
    worker_threads[i].join();
  }

  assert(0 == zmq_close(to_swap_prod));
  assert(0 == zmq_ctx_term(ctx));
}

int main(int argc, char *argv[]) {
  uint64_t len = 16;
  if (1 < argc) {
    len = strtoull(argv[1], NULL, 0);
  }
  const uint64_t len_roundup = roundup64(len);
  if (len_roundup > MAX_ARRLEN) {
    std::cout << "Please raise MAX_ARRLEN (" << MAX_ARRLEN << " now)\n";
    return 0;
  }
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
