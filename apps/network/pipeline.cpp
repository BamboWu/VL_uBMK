#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "threading.h"
#include "timing.h"
#include "utils.hpp"

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifdef VLINLINE
#include "vl/vl_inline.h"
#elif VL
#include "vl/vl.h"
#elif CAF
#include "caf.h"
#elif ZMQ
#include <zmq.h>
#elif BLFQ
#include <boost/lockfree/queue.hpp>
#endif

#ifdef BLFQ
boost::lockfree::queue< Packet* > q01(1);
boost::lockfree::queue< Packet* > q12(1);
boost::lockfree::queue< Packet* > q23(1);
boost::lockfree::queue< Packet* > q30(1);
#elif ZMQ
void *ctx;
int zmq_hwm;
#else
int q01 = 1; // id for the queue connecting stage 0 and stage 1, 1:N
int q12 = 2; // id for the queue connecting stage 1 and stage 2, N:M
int q23 = 3; // id for the queue connecting stage 2 and stage 3, M:1
int q30 = 4; // id for the queue connecting stage 3 and mempool, 1:1
#endif

uint64_t num_packets = 16;

std::atomic<int> ready;

union {
  bool done; // to tell other threads we are done, only stage 4 thread writes
  char pad[64];
} volatile __attribute__((aligned(64))) lock = { .done = false };

void* stage0(void* args) {
  int desired_core = (uint64_t) args;
  //pinAtCoreFromList(desired_core);

  size_t cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q30, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_byte_vl_as_producer(q01, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q30, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_caf(q01, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
#elif ZMQ
  void *prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(prod);
  assert(0 == zmq_setsockopt(prod, ZMQ_SNDHWM, &zmq_hwm, sizeof(zmq_hwm)));
  assert(0 == zmq_bind(prod, "inproc://q01"));
  void *cons = zmq_socket(ctx, ZMQ_PULL);
  assert(cons);
  assert(0 == zmq_setsockopt(cons, ZMQ_RCVHWM, &zmq_hwm, sizeof(zmq_hwm)));
  assert(0 == zmq_connect(cons, "inproc://q30"));
  int64_t sz_cnt;
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) {
    for (long i = 0; (10 * desired_core) > i; ++i) {
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
  };

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire packet header points from pool
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#elif ZMQ
    sz_cnt = zmq_recv(cons, pkts, sizeof(Packet*) * BULK_SIZE, ZMQ_DONTWAIT);
    if (0 > sz_cnt) {
      sz_cnt = 0;
    }
    cnt = sz_cnt / sizeof(Packet*);
#elif BLFQ
    cnt = 0;
    for (int npops = BULK_SIZE; 0 < npops; --npops) {
      if (q30.pop(pkts[cnt])) {
        cnt++;
      }
    }
#endif

    if (cnt) { // pkts now have valid pointers
      for (uint64_t j = 0; cnt > j; ++j) {
        pkts[j]->ipheader.data.srcIP = i + j;
        pkts[j]->ipheader.data.dstIP = ~i;
        pkts[j]->ipheader.data.checksumIP =
          pkts[j]->ipheader.data.srcIP ^ pkts[j]->ipheader.data.dstIP;
        pkts[j]->tcpheader.data.srcPort = (uint16_t)num_packets;
        pkts[j]->tcpheader.data.dstPort = (uint16_t)num_packets;
        pkts[j]->tcpheader.data.checksumTCP =
          pkts[j]->tcpheader.data.srcPort ^ pkts[j]->tcpheader.data.dstPort;
#ifdef CAF_PREPUSH
        caf_prepush((void*)pkts[j], HEADER_SIZE);
#endif
      }
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)pkts, cnt * sizeof(Packet*));
#elif CAF
      uint64_t j = 0; // successfully pushed count
      do {
        j += caf_push_bulk(&prod, (uint64_t*)&pkts[j], cnt - j);
      } while (j < cnt);
#elif ZMQ
      assert(sz_cnt == zmq_send(prod, pkts, sz_cnt, 0));
#elif BLFQ
      uint64_t j = 0;
      do {
        j += q01.bounded_push(pkts[j]);
      } while (j < cnt);
#endif
      i += cnt;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }

  return NULL;
}

void* stage1(void* args) {
  int desired_core = (uint64_t) args;
  //pinAtCoreFromList(desired_core);

  uint16_t checksum = 0;
  uint64_t corrupted = 0;
  size_t cnt = 0;
  bool done = false;
  Packet *pkts[BULK_SIZE] = { NULL };

  // get rid of unused warning
  checksum = checksum;
  corrupted = corrupted;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q01, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_byte_vl_as_producer(q12, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q01, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_caf(q12, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
#elif ZMQ
  void *prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(prod);
  assert(0 == zmq_setsockopt(prod, ZMQ_SNDHWM, &zmq_hwm, sizeof(zmq_hwm)));
  void *cons = zmq_socket(ctx, ZMQ_PULL);
  assert(cons);
  assert(0 == zmq_setsockopt(cons, ZMQ_RCVHWM, &zmq_hwm, sizeof(zmq_hwm)));
  int64_t sz_cnt;
#endif

#ifdef ZMQ
  if (2 == desired_core) { // the leader of the stage 1 workers
    assert(0 == zmq_bind(prod, "inproc://q12"));
    while (1 != ready.load()) { // need to let stage0 create q01 socket first
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
  } else {
    while (2 > ready.load()) {
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
    // TODO: ZMQ_PUSH/PULL handles either 1:M or M:1, does not work here
    assert(0 == zmq_connect(prod, "inproc://q12"));
  }
  assert(0 == zmq_connect(cons, "inproc://q01"));
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) {
    for (long i = 0; (10 * desired_core) > i; ++i) {
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
  }

  while (!done) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#elif ZMQ
    sz_cnt = zmq_recv(cons, pkts, sizeof(Packet*) * BULK_SIZE, ZMQ_DONTWAIT);
    if (0 > sz_cnt) {
      sz_cnt = 0;
    }
    cnt = sz_cnt / sizeof(Packet*);
#elif BLFQ
    cnt = 0;
    for (int npops = BULK_SIZE; 0 < npops; --npops) {
      if (q01.pop(pkts[cnt])) {
        cnt++;
      }
    }
#endif

    if (cnt) { // pkts has valid pointers
      // process header information
      for (uint64_t i = 0; cnt > i; ++i) {
#ifdef STAGE1_READ
        if (pkts[i]->ipheader.data.checksumIP !=
            (pkts[i]->ipheader.data.srcIP ^ pkts[i]->ipheader.data.dstIP)) {
          corrupted++;
        }
#endif
#ifdef STAGE1_WRITE
        pkts[i]->ipheader.data.checksumIP = (uint16_t)corrupted;
#endif
#ifdef CAF_PREPUSH
        caf_prepush((void*)pkts[i], HEADER_SIZE);
#endif
      }

      // after processing, propogate the packet to the next stage
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)pkts, cnt * sizeof(Packet*));
#elif CAF
      uint64_t i = 0; // sucessufully pushed count
      do {
        i += caf_push_bulk(&prod, (uint64_t*)&pkts[i], cnt - i);
      } while (i < cnt);
#elif ZMQ
      assert(sz_cnt == zmq_send(prod, pkts, sz_cnt, 0));
#elif BLFQ
      uint64_t j = 0;
      do {
        j += q12.bounded_push(pkts[j]);
      } while (j < cnt);
#endif
      continue;
    }

    done = lock.done;
  }

  return NULL;
}

void* stage2(void* args) {
  int desired_core = (uint64_t) args;
  pinAtCoreFromList(desired_core);

  uint16_t checksum = 0;
  uint64_t corrupted = 0;
  size_t cnt = 0;
  bool done = false;
  Packet *pkts[BULK_SIZE] = { NULL };

  // get rid of unused warning
  checksum = checksum;
  corrupted = corrupted;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q12, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_byte_vl_as_producer(q23, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q12, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return NULL;
  }
  if (open_caf(q23, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return NULL;
  }
#elif ZMQ
  void *prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(prod);
  assert(0 == zmq_setsockopt(prod, ZMQ_SNDHWM, &zmq_hwm, sizeof(zmq_hwm)));
  assert(0 == zmq_connect(prod, "inproc://q23"));
  void *cons = zmq_socket(ctx, ZMQ_PULL);
  assert(cons);
  assert(0 == zmq_setsockopt(cons, ZMQ_RCVHWM, &zmq_hwm, sizeof(zmq_hwm)));
  int64_t sz_cnt;
#endif

#ifdef ZMQ
  while (2 > ready.load()) {
    // need to let stage1 leader create q12 socket first
    __asm__ volatile("\
        nop \n\
        nop \n\
        nop \n\
        nop \n\
        nop \n\
        "
        :
        :
        :
        );
  }
  assert(0 == zmq_connect(cons, "inproc://q12"));
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) {
    for (long i = 0; (10 * desired_core) > i; ++i) {
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
  }

  while (!done) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#elif ZMQ
    sz_cnt = zmq_recv(cons, pkts, sizeof(Packet*) * BULK_SIZE, ZMQ_DONTWAIT);
    if (0 > sz_cnt) {
      sz_cnt = 0;
    }
    cnt = sz_cnt / sizeof(Packet*);
#elif BLFQ
    cnt = 0;
    for (int npops = BULK_SIZE; 0 < npops; --npops) {
      if (q12.pop(pkts[cnt])) {
        cnt++;
      }
    }
#endif

    if (cnt) { // pkts has valid pointers
      // process header information
      for (uint64_t i = 0; cnt > i; ++i) {
#ifdef STAGE2_READ
        if (pkts[i]->tcpheader.data.checksumTCP !=
            (pkts[i]->tcpheader.data.srcPort ^
             pkts[i]->tcpheader.data.dstPort)) {
          corrupted++;
        }
#endif
#ifdef STAGE2_WRITE
        pkts[i]->tcpheader.data.checksumTCP = (uint16_t)corrupted;
#endif
#ifdef CAF_PREPUSH
        caf_prepush((void*)pkts[i], HEADER_SIZE);
#endif
      }

      // after processing, propogate the packet to the next stage
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)pkts, cnt * sizeof(Packet*));
#elif CAF
      uint64_t i = 0; // sucessufully pushed count
      do {
        i += caf_push_bulk(&prod, (uint64_t*)&pkts[i], cnt - i);
      } while (i < cnt);
#elif ZMQ
      assert(sz_cnt == zmq_send(prod, pkts, sz_cnt, 0));
#elif BLFQ
      uint64_t j = 0;
      do {
        j += q23.bounded_push(pkts[j]);
      } while (j < cnt);
#endif
      continue;
    }

    done = lock.done;
  }

  return NULL;
}

int main(int argc, char *argv[]) {

  char core_list[] = "0-3";

  uint64_t core_id = 1;
  size_t cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };

  if (1 < argc) {
    num_packets = atoi(argv[1]);
  }
  if (2 < argc) {
    parseCoreList(argv[2]);
  } else {
    parseCoreList(core_list);
  }
#if ZMQ
  ctx = zmq_ctx_new();
  assert(ctx);
  zmq_hwm = POOL_SIZE;
  if (3 < argc) {
    zmq_hwm = atoi(argv[3]);
  }
#elif BLFQ
  size_t blfq_size = POOL_SIZE;
  if (3 < argc) {
    blfq_size = atoi(argv[3]);
  }
  q01.reserve(blfq_size);
  q12.reserve(blfq_size);
  q23.reserve(blfq_size);
  q30.reserve(blfq_size);
#endif
  pinAtCoreFromList(0);
  printf("%s 1-%d-%d-1 %d bulk %lu pkts %d pool\n",
         argv[0], NUM_STAGE1, NUM_STAGE2, BULK_SIZE, num_packets, POOL_SIZE);

#ifdef VL
  q01 = mkvl();
  if (0 > q01) {
    printf("\033[91mFAILED:\033[0m q01 = mkvl() return %d\n", q01);
    return -1;
  }
  q12 = mkvl();
  if (0 > q12) {
    printf("\033[91mFAILED:\033[0m q12 = mkvl() return %d\n", q12);
    return -1;
  }
  q23 = mkvl();
  if (0 > q23) {
    printf("\033[91mFAILED:\033[0m q23 = mkvl() return %d\n", q23);
    return -1;
  }
  q30 = mkvl();
  if (0 > q30) {
    printf("\033[91mFAILED:\033[0m q30 = mkvl() return %d\n", q30);
    return -1;
  }
  // open endpoints
  vlendpt_t cons, prod;
  if (open_byte_vl_as_consumer(q23, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), cons\n", __func__);
    return -1;
  }
  if (open_byte_vl_as_producer(q30, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), prod\n", __func__);
    return -1;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  if (open_caf(q23, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), cons\n", __func__);
    return -1;
  }
  if (open_caf(q30, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), prod\n", __func__);
    return -1;
  }
#elif ZMQ
  void *prod = zmq_socket(ctx, ZMQ_PUSH);
  assert(prod);
  assert(0 == zmq_setsockopt(prod, ZMQ_SNDHWM, &zmq_hwm, sizeof(zmq_hwm)));
  assert(0 == zmq_bind(prod, "inproc://q30"));
  void *cons = zmq_socket(ctx, ZMQ_PULL);
  assert(cons);
  assert(0 == zmq_setsockopt(cons, ZMQ_RCVHWM, &zmq_hwm, sizeof(zmq_hwm)));
  assert(0 == zmq_bind(cons, "inproc://q23"));
#endif

  ready = 0;
  pthread_t slave_threads[2+NUM_STAGE1+NUM_STAGE2];
  threadCreate(&slave_threads[core_id], NULL, stage0, (void*)core_id, core_id);
  core_id++;
  for (int i = 0; NUM_STAGE1 > i; ++i) {
    threadCreate(&slave_threads[core_id], NULL, stage1, (void*)core_id,
            core_id);
    core_id++;
  }
  for (int i = 0; NUM_STAGE2 > i; ++i) {
    threadCreate(&slave_threads[core_id], NULL, stage2, (void*)core_id,
            core_id);
    core_id++;
  }

  void *mempool = malloc(POOL_SIZE << 11); // POOL_SIZE 2KB memory blocks
  void *headerpool = malloc(POOL_SIZE * HEADER_SIZE);

  while ((1 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) {
    for (long i = 0; 10 > i; ++i) {
      __asm__ volatile("\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          nop \n\
          "
          :
          :
          :
          );
    }
  };

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

  ready++;

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  // add allocated memory blocks into pool
  for (int i = 0; POOL_SIZE > i;) {
    size_t j = 0;
    while (BULK_SIZE > j && POOL_SIZE > i) {
      pkts[j] = (Packet*)((uint64_t)headerpool + (i * HEADER_SIZE));
      pkts[j]->payload = (void*)((uint64_t)mempool + (i << 11));
      j++;
    }
#ifdef VL
    line_vl_push_strong(&prod, (uint8_t*)pkts, sizeof(Packet*) * j);
#elif CAF
    assert(j == caf_push_bulk(&prod, (uint64_t*)pkts, j));
#elif ZMQ
    assert((int64_t)(sizeof(Packet*) * j) ==
            zmq_send(prod, pkts, sizeof(Packet*) * j, 0));
#elif BLFQ
    size_t k = 0;
    while (j > k) {
      k += q30.bounded_push(pkts[k]);
    }
#endif
    i += j;
  }

  size_t ncnt2send = (num_packets - POOL_SIZE) * sizeof(Packet*);
  size_t npktsrecvd = 0;
  while (num_packets > npktsrecvd) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#elif ZMQ
    cnt = zmq_recv(cons, pkts, sizeof(Packet*) * BULK_SIZE, ZMQ_DONTWAIT);
    if ((sizeof(Packet*) * BULK_SIZE) < cnt) {
      cnt = 0;
    }
#elif BLFQ
    cnt = 0;
    for (int npops = BULK_SIZE; 0 < npops; --npops) {
      if (q23.pop(pkts[cnt])) {
        cnt++;
      }
    }
    cnt *= sizeof(Packet*);
#endif

    if (cnt) { // valid packets pointers in pkts
      if (ncnt2send) {
        size_t cnt2send = (ncnt2send > cnt) ? cnt : ncnt2send;
#ifdef VL
        line_vl_push_weak(&prod, (uint8_t*)pkts, cnt2send);
#elif CAF
        uint64_t j = 0; // successfully pushed count
        do {
          j += caf_push_bulk(&prod, (uint64_t*)&pkts[j], cnt2send - j);
        } while (j < cnt2send);
#elif ZMQ
        assert((int64_t)cnt2send == zmq_send(prod, pkts, cnt2send, 0));
#elif BLFQ
        uint64_t j = 0;
        uint64_t push2do = cnt2send / sizeof(Packet*);
        do {
          j += q30.bounded_push(pkts[j]);
        } while (j < push2do);
#endif
        ncnt2send -= cnt2send;
      }
      cnt /= sizeof(Packet*);
      npktsrecvd += cnt;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }

  lock.done = true;

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  for (int i = 1; (2 + NUM_STAGE1 + NUM_STAGE2) > i; ++i) {
    pthread_join(slave_threads[i], NULL);
  }

  free(mempool);
  return 0;
}
