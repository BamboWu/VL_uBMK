#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <stdlib.h>
#include <stdint.h>

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

  int errorcode;
  size_t cnt;
  Packet pkt;
  bool valid;
  void *payload;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(q30, &cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(q01, &prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire a 2MB memory from pool
#ifdef VL
    line_vl_pop_non(&cons, (uint8_t*)&payload, &cnt);
    valid = (sizeof(void *) == cnt);
#endif

    if (valid) { // payload now points to a 2MB memory from pool
      pkt.payload = payload;
      pkt.srcIP = i;
      pkt.dstIP = ~i;
      pkt.checksumIP = pkt.srcIP ^ pkt.dstIP;
      pkt.srcPort = (uint16_t)num_packets;
      pkt.dstPort = (uint16_t)num_packets;
      pkt.checksumTCP = pkt.srcPort ^ pkt.dstPort;
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)&pkt, HEADER_SIZE);
#endif
      i++;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)&pkt, 0); // help flushing
#endif
  }

}

void stage1(int desired_core) {
  setAffinity(desired_core);

  int errorcode;
  size_t cnt;
  Packet pkt;
  bool valid;
  bool done = false;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(q01, &cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(q12, &prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  while (!done) {
    // try to acquire a packet
#ifdef VL
    line_vl_pop_non(&cons, (uint8_t*)&pkt, &cnt);
    valid = (HEADER_SIZE == cnt);
#endif

    if (valid) { // pkt is valid
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)&pkt, HEADER_SIZE);
#endif
      continue;
    }

    done = lock.done;
#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)&pkt, 0); // help flushing
#endif
  }

}

void stage2(int desired_core) {
  setAffinity(desired_core);

  int errorcode;
  size_t cnt;
  Packet pkt;
  bool valid;
  bool done = false;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(q12, &cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(q23, &prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  while (!done) {
    // try to acquire a packet
#ifdef VL
    line_vl_pop_non(&cons, (uint8_t*)&pkt, &cnt);
    valid = (HEADER_SIZE == cnt);
#endif

    if (valid) { // pkt is valid
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)&pkt, HEADER_SIZE);
#endif
      continue;
    }

    done = lock.done;
#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)&pkt, 0); // help flushing
#endif
  }

}

int main(int argc, char *argv[]) {

  setAffinity(0);

  int core_id = 1;
  Packet pkt;
  size_t cnt;
  bool valid;

  if (1 < argc) {
    num_packets = atoi(argv[1]);
  }
  printf("%s 1-%d-%d-1 %lu pkts %d pool\n",
         argv[0], NUM_STAGE1, NUM_STAGE2, num_packets, POOL_SIZE);

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

  void *mempool = malloc(POOL_SIZE << 21); // POOL_SIZE 2MB memory blocks

  // add allocated memory blocks into pool
  for (int i = 0; POOL_SIZE > i; ++i) {
    void *payload = (void *)((uint64_t)mempool + (i << 21));
#ifdef VL
    line_vl_push_strong(&prod, (uint8_t*)&payload, sizeof(void *));
#endif
  }

  ready++;
  while ((2 + NUM_STAGE1 + NUM_STAGE2) != ready.load()) { /** spin **/ };

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire a packet
#ifdef VL
    line_vl_pop_non(&cons, (uint8_t*)&pkt, &cnt);
    valid = (HEADER_SIZE == cnt);
#endif

    if (valid) { // payload now points to a 2MB memory from pool
#ifdef VL
      line_vl_push_weak(&prod, (uint8_t*)&pkt.payload, sizeof(void *));
#endif
      i++;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)&pkt, 0); // help flushing
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
