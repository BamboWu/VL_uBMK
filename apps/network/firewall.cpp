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
int q1c = 2; // id for the queue connecting stage 1 and correct, N:1
int q1m = 3; // id for the queue connecting stage 1 and mistake, N:1
int qp0 = 4; // id for the memory pool queue, 2:1
uint64_t num_packets = 16;
uint64_t num_correct;
uint64_t num_mistake;

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
  if (open_byte_vl_as_consumer(qp0, &cons, 1)) {
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
  if (open_caf(qp0, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(q01, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((3 + NUM_STAGE1) != ready.load()) { /** spin **/ };

  for (uint64_t i = 0; num_packets > i;) {
    // try to acquire packet header points from pool
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    if (cnt < bulk_size) {
      size_t tmp = bulk_size - cnt;
      line_vl_pop_non(&cons, (uint8_t*)&pkts[cnt], &tmp);
      cnt += tmp;
    }
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
    if (cnt < BULK_SIZE) {
        cnt += caf_pop_bulk(&cons, (uint64_t*)&pkts[cnt], BULK_SIZE - cnt);
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
#endif
      i += cnt;
      continue;
    }

#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }
  printf("done\n");

}

void stage1(int desired_core) {
  setAffinity(desired_core);

  uint16_t checksum = 0;
  uint64_t corrupted = 0;
  size_t cnt = 0;
  bool done = false;
  uint64_t pktscidx = 0;
  uint64_t pktsmidx = 0;
  uint64_t mistake_cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };
  Packet *pktsc[BULK_SIZE] = { NULL }; // packets to stage2 correct
  Packet *pktsm[BULK_SIZE] = { NULL }; // packets to stage2 mistake

  // get rid of unused warning
  checksum = checksum;
  corrupted = corrupted;

#ifdef VL
  vlendpt_t cons, prodc, prodm;
  // open endpoints
  if (open_byte_vl_as_consumer(q01, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_byte_vl_as_producer(q1c, &prodc, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prodc\n", __func__, desired_core);
    return;
  }
  if (open_byte_vl_as_producer(q1m, &prodm, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prodm\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prodc, prodm;
  // open endpoints
  if (open_caf(q01, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(q1c, &prodc)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prodc\n", __func__, desired_core);
    return;
  }
  if (open_caf(q1m, &prodm)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prodm\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((3 + NUM_STAGE1) != ready.load()) { /** spin **/ };

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
        if (pkts[i]->ipheader.data.checksumIP !=
            (pkts[i]->ipheader.data.srcIP ^ pkts[i]->ipheader.data.dstIP) ||
            pkts[i]->tcpheader.data.checksumTCP !=
            (pkts[i]->tcpheader.data.srcPort ^
             pkts[i]->tcpheader.data.dstPort)) {
          pktsm[pktsmidx++] = pkts[i];
        } else {
          pktsc[pktscidx++] = pkts[i];
        }
#ifdef CAF_PREPUSH
        caf_prepush((void*)pkts[i], HEADER_SIZE);
#endif
      }

      // after processing, propogate the packet to the next stage
      if (pktscidx) {
#ifdef VL
        line_vl_push_weak(&prodc, (uint8_t*)pktsc, pktscidx * sizeof(Packet*));
#elif CAF
        uint64_t i = 0; // sucessufully pushed count
        do {
          i += caf_push_bulk(&prodc, (uint64_t*)&pktsc[i], pktscidx - i);
        } while (i < pktscidx);
#endif
        pktscidx = 0;
      }
      if (BULK_SIZE == pktsmidx || MISTAKE_GATHER_RETRY <= mistake_cnt) {
#ifdef VL
        line_vl_push_weak(&prodm, (uint8_t*)pktsm, pktsmidx * sizeof(Packet*));
#elif CAF
        uint64_t i = 0; // sucessufully pushed count
        do {
          i += caf_push_bulk(&prodm, (uint64_t*)&pktsm[i], pktsmidx - i);
        } while (i < pktsmidx);
#endif
        pktsmidx = 0;
        mistake_cnt = 0;
      } else {
        mistake_cnt++;
      }
      continue;
    }

    done = lock.done;
#ifdef VL
    line_vl_push_non(&prodc, (uint8_t*)pktsc, 0); // help flushing
    line_vl_push_non(&prodm, (uint8_t*)pktsm, 0); // help flushing
#endif
  }

}

void stage2correct(int desired_core) {
  setAffinity(desired_core);

  uint16_t checksum = 0;
  uint64_t corrupted = 0;
  size_t cnt = 0;
  size_t cnt_sum = 0;
  bool done = false;
  Packet *pkts[BULK_SIZE] = { NULL };

  // get rid of unused warning
  checksum = checksum;
  corrupted = corrupted;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q1c, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_byte_vl_as_producer(qp0, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q1c, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(qp0, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((3 + NUM_STAGE1) != ready.load()) { /** spin **/ };

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
      cnt_sum += cnt;
      // process header information
      num_correct += cnt;
      for (uint64_t i = 0; cnt > i; ++i) {
#ifdef CORRECT_READ
        if (pkts[i]->tcpheader.data.checksumTCP !=
            (pkts[i]->tcpheader.data.srcPort ^
             pkts[i]->tcpheader.data.dstPort)) {
          corrupted++;
        }
#endif
#ifdef CORRECT_WRITE
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
#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }

  num_correct = cnt_sum;

}

void stage2mistake(int desired_core) {
  setAffinity(desired_core);

  uint16_t checksum = 0;
  uint64_t corrupted = 0;
  size_t cnt = 0;
  size_t cnt_sum = 0;
  bool done = false;
  Packet *pkts[BULK_SIZE] = { NULL };

  // get rid of unused warning
  checksum = checksum;
  corrupted = corrupted;

#ifdef VL
  vlendpt_t cons, prod;
  // open endpoints
  if (open_byte_vl_as_consumer(q1m, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_byte_vl_as_producer(qp0, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
  uint8_t *pktsbyte = (uint8_t*)pkts;
#elif CAF
  cafendpt_t cons, prod;
  // open endpoints
  if (open_caf(q1m, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d cons\n", __func__, desired_core);
    return;
  }
  if (open_caf(qp0, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), T%d prod\n", __func__, desired_core);
    return;
  }
#endif

  ready++;
  while ((3 + NUM_STAGE1) != ready.load()) { /** spin **/ };

  while (!done) {
    // try to acquire a packet
#ifdef VL
    cnt = bulk_size;
    line_vl_pop_non(&cons, (uint8_t*)pkts, &cnt);
    for (uint64_t j = 0; cnt < bulk_size && MISTAKE_GATHER_RETRY > j; ++j) {
      size_t tmp = bulk_size - cnt;
      line_vl_pop_non(&cons, &pktsbyte[cnt], &tmp);
      cnt += tmp;
    }
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
#endif

    if (cnt) { // pkts has valid pointers
      cnt_sum += cnt;
      // process header information
      for (uint64_t i = 0; cnt > i; ++i) {
#ifdef MISTAKE_READ
        if (pkts[i]->tcpheader.data.checksumTCP !=
            (pkts[i]->tcpheader.data.srcPort ^
             pkts[i]->tcpheader.data.dstPort)) {
          corrupted++;
        }
#endif
#ifdef MISTAKE_WRITE
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
#ifdef VL
    line_vl_push_non(&prod, (uint8_t*)pkts, 0); // help flushing
#endif
  }

  num_mistake = cnt_sum;

}

int main(int argc, char *argv[]) {

  setAffinity(0);

  int core_id = 1;
  size_t cnt = 0;
  Packet *pkts[BULK_SIZE] = { NULL };

  if (1 < argc) {
    num_packets = atoi(argv[1]);
  }
  printf("%s 1-%d-1-1 %d bulk %lu pkts %d pool\n",
         argv[0], NUM_STAGE1, BULK_SIZE, num_packets, POOL_SIZE);

#ifdef VL
  q01 = mkvl();
  if (0 > q01) {
    printf("\033[91mFAILED:\033[0m q01 = mkvl() return %d\n", q01);
    return -1;
  }
  q1c = mkvl();
  if (0 > q1c) {
    printf("\033[91mFAILED:\033[0m q1c = mkvl() return %d\n", q1c);
    return -1;
  }
  q1m = mkvl();
  if (0 > q1m) {
    printf("\033[91mFAILED:\033[0m q1m = mkvl() return %d\n", q1m);
    return -1;
  }
  qp0 = mkvl();
  if (0 > qp0) {
    printf("\033[91mFAILED:\033[0m qp0 = mkvl() return %d\n", qp0);
    return -1;
  }
  // open endpoints
  vlendpt_t cons, prod;
  if (open_byte_vl_as_consumer(qp0, &cons, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), cons\n", __func__);
    return -1;
  }
  if (open_byte_vl_as_producer(q01, &prod, 1)) {
    printf("\033[91mFAILED:\033[0m %s(), prod\n", __func__);
    return -1;
  }
  const size_t bulk_size = BULK_SIZE * sizeof(Packet*);
  uint8_t *pktsbyte = (uint8_t*)pkts;
#elif CAF
  cafendpt_t cons, prod;
  if (open_caf(qp0, &cons)) {
    printf("\033[91mFAILED:\033[0m %s(), cons\n", __func__);
    return -1;
  }
  if (open_caf(q01, &prod)) {
    printf("\033[91mFAILED:\033[0m %s(), prod\n", __func__);
    return -1;
  }
  cnt = cnt;
#endif

  ready = 0;
  std::vector<thread> slave_threads;
  for (int i = 0; NUM_STAGE1 > i; ++i) {
    slave_threads.push_back(thread(stage1, core_id++));
  }
  slave_threads.push_back(thread(stage2correct, core_id++));
  slave_threads.push_back(thread(stage2mistake, core_id++));

  void *mempool = malloc(POOL_SIZE << 11); // POOL_SIZE 2KB memory blocks
  void *headerpool = malloc(POOL_SIZE * HEADER_SIZE);

  // add allocated memory blocks into pool
  for (int i = 0; POOL_SIZE > i;) {
    size_t j = 0;
    while (BULK_SIZE > j && POOL_SIZE > i) {
      pkts[j] = (Packet*)((uint64_t)headerpool + ((i + j) * HEADER_SIZE));
      pkts[j]->payload = (void*)((uint64_t)mempool + ((i + j) << 11));
      pkts[j]->ipheader.data.srcIP = (uint16_t)i;
      pkts[j]->ipheader.data.dstIP = (uint16_t)j;
      pkts[j]->ipheader.data.checksumIP =
        (0 == (i + j) % 8) ? i ^ (j + 1) : i ^ j;
      pkts[j]->tcpheader.data.srcPort = (uint16_t)num_packets;
      pkts[j]->tcpheader.data.dstPort = (uint16_t)num_packets;
      pkts[j]->tcpheader.data.checksumTCP =
        pkts[j]->tcpheader.data.srcPort ^ pkts[j]->tcpheader.data.dstPort;
#ifdef CAF_PREPUSH
      caf_prepush((void*)pkts[j], HEADER_SIZE);
#endif
      j++;
    }
#ifdef VL
    line_vl_push_strong(&prod, (uint8_t*)pkts, sizeof(Packet*) * j);
#elif CAF
    assert(j == caf_push_bulk(&prod, (uint64_t*)pkts, j));
#endif
    i += j;
  }

  while ((2 + NUM_STAGE1) != ready.load()) { /** spin **/ };
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
    for (uint64_t j = 0; cnt < bulk_size && BULK_SIZE > j; ++j) {
      size_t tmp = bulk_size - cnt;
      line_vl_pop_non(&cons, &pktsbyte[cnt], &tmp);
      cnt += tmp;
    }
    cnt /= sizeof(Packet*);
#elif CAF
    cnt = caf_pop_bulk(&cons, (uint64_t*)pkts, BULK_SIZE);
    if (cnt < BULK_SIZE) {
        cnt += caf_pop_bulk(&cons, (uint64_t*)&pkts[cnt], BULK_SIZE - cnt);
    }
#endif

    if (cnt) { // valid packets pointers in pkts
      for (uint64_t j = 0; cnt > j; ++j) {
        pkts[j]->ipheader.data.srcIP = (uint16_t)i;
        pkts[j]->ipheader.data.dstIP = (uint16_t)j;
        pkts[j]->ipheader.data.checksumIP =
          (0 == (i + j) % 8) ? i ^ (j + 1) : i ^ j;
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

  lock.done = true;

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  for (int i = 0; (2 + NUM_STAGE1) > i; ++i) {
    slave_threads[i].join();
  }

  std::cout << num_correct << " correct packet(s) and " <<
      num_mistake << " corrupted packet(s)\n";

  free(mempool);
  return 0;
}
