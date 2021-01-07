#include <iostream>
#include <thread>
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

using std::thread;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifdef VL
#include "vl/vl.h"
#elif CAF
#include "caf.h"
#endif

int q01 = 1; // id for the queue connecting stage 0 and stage 1, 1:N
int q12 = 2; // id for the queue connecting stage 1 and stage 2, N:M
int q23 = 3; // id for the queue connecting stage 2 and stage 3, M:1
int q30 = 4; // id for the queue connecting stage 3 and mempool, 1:1
uint64_t num_packets = 16;

std::atomic<int> ready;

union {
  bool done; // to tell other threads we are done, only stage 4 thread writes
  char pad[64];
} volatile __attribute__((aligned(64))) lock = { .done = false };

void stage0(int desired_core) {
  setAffinity(desired_core);

  size_t cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q30, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_byte_vl_as_producer(q01, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q30, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(q01, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire packet header points from pool
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
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
#endif
      i += cnt;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }

}

void stage1(int desired_core) {
  setAffinity(desired_core);

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
    return;
  }
  if (open_byte_vl_as_producer(q12, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q01, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(q12, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  while (!done) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
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
#endif
      continue;
    }

    done = lock.done;
  }

}

void stage2(int desired_core) {
  setAffinity(desired_core);

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
    return;
  }
  if (open_byte_vl_as_producer(q23, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q12, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(q23, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  while (!done) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
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
#endif
      continue;
    }

    done = lock.done;
  }

}

int main(int argc, char *argv[]) {

  setAffinity(0);

  int core_id = 1;
  size_t cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };

  if (1 < argc) {
    num_packets = atoi(argv[1]);
  }
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
#endif

  ready = 0;
  std::vector<thread> slave_threads;
  slave_threads.push_back(thread(stage0, core_id++));
  for (int i = 0; NUM_STAGE1 > i; ++i) {
    slave_threads.push_back(thread(stage1, core_id++));
  }
  for (int i = 0; NUM_STAGE2 > i; ++i) {
    slave_threads.push_back(thread(stage2, core_id++));
  }

  void *mempool = malloc(POOL_SIZE << 11); // POOL_SIZE 2KB memory blocks
  void *headerpool = malloc(POOL_SIZE * HEADER_SIZE);

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
#endif
    i += j;
  }

  while ((1 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };
  ready++;

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#endif

    if (cnt) { // valid packets pointers in pkts
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)pkts, cnt);
      cnt /= sizeof(Packet*);
#elif CAF
      uint64_t j = 0; // successfully pushed count
      do {
        j += caf_push_bulk(&prod, (uint64_t*)&pkts[j], cnt - j);
      } while (j < cnt);
#endif
      i += cnt;
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

  for (int i = 0; (1 + NUM_STAGE1 + NUM_STAGE2) > i; ++i) {
    slave_threads[i].join();
  }

  free(mempool);
  return 0;
}
