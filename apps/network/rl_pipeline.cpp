#include <iostream>
#include <chrono>
#include <atomic>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#include <raft>

#include "timing.h"
#include "utils.hpp"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

struct PktPtr {
  PktPtr() {
    pkt = nullptr;
  }
  PktPtr(Packet *the_pkt) : pkt(the_pkt) {}
  PktPtr(const PktPtr &other) : pkt(other.pkt) {}
  Packet *pkt;
};

class stage0 : public raft::Kernel {
public:
  stage0(const std::size_t num_packets, const int fanout) :
      raft::Kernel(), npackets(num_packets) {
#if RAFTLIB_ORIG
      for (int i = 0; fanout > i; ++i) {
        add_output<PktPtr>(port_name_of_i(i));
      }
#else
      add_output<PktPtr>("0"_port);
#endif
    void *mem_pool = memalign(L1D_CACHE_LINE_SIZE, POOL_SIZE << 1);
    void *hdr_pool = memalign(L1D_CACHE_LINE_SIZE,
                              POOL_SIZE * HEADER_SIZE);
    for (int i = 0; POOL_SIZE > i; ++i) {
      pkt_pool[i] = (Packet*)((uint64_t)hdr_pool + i * HEADER_SIZE);
      pkt_pool[i]->payload = (void*)((uint64_t)mem_pool + (i << 11));
    }
  }

  ~stage0() {
    free(pkt_pool[0]->payload);
    free(pkt_pool[0]);
  }

#if RAFTLIB_ORIG
  virtual raft::kstatus run() {
    for (auto &port : (this)->output) {
      if (!port.space_avail()) {
        continue;
      }
      port.push(genpkt());
      if (npackets < ++cnt) {
        return raft::kstatus::stop;
      }
    }
    return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
    bufOut.push(genpkt());
    if (npackets < ++cnt) {
      return raft::kstatus::stop;
    }
    return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

private:

  PktPtr genpkt() {
    Packet * pkt = pkt_pool[cnt % POOL_SIZE];
    pkt->ipheader.data.srcIP = cnt;
    pkt->ipheader.data.dstIP = ~cnt;
    pkt->ipheader.data.checksumIP =
      pkt->ipheader.data.srcIP ^ pkt->ipheader.data.dstIP;
    pkt->tcpheader.data.srcPort = (uint16_t)npackets;
    pkt->tcpheader.data.dstPort = (uint16_t)npackets;
    pkt->tcpheader.data.checksumTCP =
      pkt->tcpheader.data.srcPort ^ pkt->tcpheader.data.dstPort;
    return PktPtr(pkt);
  }

  const std::size_t npackets;
  std::size_t cnt = 0;
  Packet *pkt_pool[POOL_SIZE];
};

class stage1 : public raft::Kernel {
public:
  stage1() : raft::Kernel() {
      add_input<PktPtr>("0"_port);
      add_output<PktPtr>("0"_port);
  }

  stage1(const stage1 &other) {
      add_input<PktPtr>("0"_port);
      add_output<PktPtr>("0"_port);
  }

#if RAFTLIB_ORIG
  CLONE();
  virtual raft::kstatus run() {
    PktPtr pktptr;
    input["0"_port].pop(pktptr);
    output["0"_port].push(process(pktptr.pkt));
    return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
    PktPtr pktptr;
    dataIn.pop(pktptr);
    bufOut.push((this)->process(pktptr.pkt));
    return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

protected:

  virtual PktPtr process(Packet *pkt) {
    uint16_t corrupted = 0;
#ifdef STAGE1_READ
      if (pkt->ipheader.data.checksumIP !=
          (pkt->ipheader.data.srcIP ^ pkt->ipheader.data.dstIP)) {
        corrupted++;
      }
#endif
#ifdef STAGE1_WRITE
      pkt->ipheader.data.checksumIP = corrupted;
#endif
      return PktPtr(pkt);
    }

};

class stage2 : public stage1 {
public:
  stage2() : stage1() {}
  stage2(const stage2 &other) : stage1() {}

#if RAFTLIB_ORIG
  CLONE();
#endif

protected:
  virtual PktPtr process(Packet *pkt) override {
    uint16_t corrupted = 0;
#ifdef STAGE2_READ
    if (pkt->tcpheader.data.checksumTCP !=
        (pkt->tcpheader.data.srcPort ^
         pkt->tcpheader.data.dstPort)) {
      corrupted++;
    }
#endif
#ifdef STAGE2_WRITE
    pkt->tcpheader.data.checksumTCP = corrupted;
#endif
    return PktPtr(pkt);
  }
};

class stage3 : public raft::Kernel {
public:
  stage3(const int fanin) : raft::Kernel() {
#if RAFTLIB_ORIG
    for (int i = 0; fanin > i; ++i)
    {
        add_input<PktPtr>(port_name_of_i(i));
    }
#else
    add_input<PktPtr>("0"_port);
#endif
  }

#if RAFTLIB_ORIG
  virtual raft::kstatus run() {
    PktPtr pktptr;
    for (auto &port : (this)->input) {
      if (0 < port.size())
      {
        port.pop(pktptr);
        corrupted += pktptr.pkt->ipheader.data.checksumIP;
        corrupted += pktptr.pkt->tcpheader.data.checksumTCP;
      }
    }
    return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
    PktPtr pktptr;
    dataIn.pop(pktptr);
    corrupted += pktptr.pkt->ipheader.data.checksumIP;
    corrupted += pktptr.pkt->tcpheader.data.checksumTCP;
    return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

  std::size_t corrupted = 0;
};


int main(int argc, char *argv[]) {

  std::size_t num_packets = 16;
  int fanout = 4;

  if (1 < argc) {
    num_packets = strtoull(argv[1], NULL, 0);
  }
  if (2 < argc) {
    fanout = atoi(argv[2]);
  }
  printf("%s 1-%d-%d-1 %lu pkts %d pool\n",
         argv[0], NUM_STAGE1, NUM_STAGE2, num_packets, POOL_SIZE);

  stage0 s0(num_packets, fanout);
  stage1 s1;
  stage2 s2;
  stage3 s3(fanout);

  raft::DAG dag;

#if RAFTLIB_ORIG
  dag += s0 <= s1 >> s2 >= s3;
#else
  dag += s0 >> s1 * fanout >> s2 * fanout >> s3;
#endif

  const auto beg(high_resolution_clock::now());
  const uint64_t beg_tsc = rdtsc();

  dag.exe<
#if RAFTLIB_ORIG
      partition_dummy,
#if USE_UT || USE_QTHREAD
      pool_schedule,
#else
      simple_schedule,
#endif
#ifdef VL
      vlalloc,
#elif STDALLOC
      stdalloc,
#else
      dynalloc,
#endif
      no_parallel
#else // NOT( RAFTLIB_ORIG )
#if RAFTLIB_MIX
      raft::RuntimeMix
#elif RAFTLIB_ONESHOT
      raft::RuntimeNewBurst
#elif RAFTLIB_CV
      raft::RuntimeFIFOCV
#else
      raft::RuntimeFIFO
#endif
#endif // RAFTLIB_ORIG
      >();

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";
  return 0;
}
