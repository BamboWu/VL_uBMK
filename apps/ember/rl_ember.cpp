#include <iostream>
#include <chrono>
#include <atomic>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>

#include <raft>

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#ifndef POOL_SIZE
#define POOL_SIZE 4096
#endif
#ifndef L1D_CACHE_LINE_SIZE
#define L1D_CACHE_LINE_SIZE 64
#endif

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

struct MsgPtr {
  MsgPtr() {
    msg = nullptr;
  }
  MsgPtr(double *ptr) : msg(ptr) {}
  MsgPtr(const MsgPtr &other) : msg(other.msg) {}
  double *msg;
};

void nopcompute(long sleep_nsec) {
  for (long i = 0; sleep_nsec > i; ++i) {
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

/* Global variables */
int pex, pey, nthreads;
std::size_t repeats;
int msgSz;
long sleep_nsec;
long sleep_nsec2;
long burst_amp;
long burst_period;
long fast_period;
long slow_period;

class ProducerK : public raft::Kernel {
public:
  ProducerK(const int fanout) : raft::Kernel(), nfanout(fanout) {
    const int ndoubles = msgSz / sizeof(double);
#if RAFTLIB_ORIG
    for (int i = 0; nfanout > i; ++i) {
      add_output<MsgPtr>(port_name_of_i(i));
    }
#else
    add_output<MsgPtr>("0"_port);
#endif
    msgpool[0] = (double*)memalign(L1D_CACHE_LINE_SIZE, POOL_SIZE * msgSz);
    for (std::size_t i = 1; POOL_SIZE > i; ++i) {
      msgpool[i] = &msgpool[0][ndoubles * i];
    }
  }

#if RAFTLIB_ORIG

  ProducerK(const ProducerK &other) : raft::Kernel(), nfanout(other.nfanout) {
    const int ndoubles = msgSz / sizeof(double);
    for (int i = 0; nfanout > i; ++i) {
      add_output<MsgPtr>(port_name_of_i(i));
    }
    msgpool[0] = (double*)memalign(L1D_CACHE_LINE_SIZE, POOL_SIZE * msgSz);
    for (std::size_t i = 1; POOL_SIZE > i; ++i) {
      msgpool[i] = &msgpool[0][ndoubles * i];
    }
  }

  CLONE();

  virtual raft::kstatus run() {
    for (auto &port : (this)->output) {
      if (!port.space_avail()) {
        continue;
      }
      auto i(cnt.fetch_add(1, std::memory_order_relaxed));
      port.push(genmsg(i));
      if (repeats < i) {
        return raft::kstatus::stop;
      }
    }
    return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
    auto i(cnt.fetch_add(1, std::memory_order_relaxed));
    bufOut.push(genmsg(i));
    if (repeats < i) {
      return raft::kstatus::stop;
    }
    return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

private:

  MsgPtr genmsg(std::size_t i) {
    long sleep_tmp =
        (((long)i % burst_period) > slow_period) ? -burst_amp : burst_amp;
    sleep_tmp += sleep_nsec2;
    if (0 < sleep_tmp) {
      nopcompute(sleep_tmp);
    }
    return MsgPtr(msgpool[i % POOL_SIZE]);
  }

  const int nfanout;
  std::atomic<std::size_t> cnt = 0;
  double *msgpool[POOL_SIZE];
};

class ConsumerK : public raft::Kernel {
public:
  ConsumerK(const int fanin) : raft::Kernel(), nfanin(fanin) {
#if RAFTLIB_ORIG
    for (int i = 0; nfanin > i; ++i) {
      add_input<MsgPtr>(port_name_of_i(i));
    }
#else
    add_input<MsgPtr>("0"_port);
#endif
  }

#if RAFTLIB_ORIG
  ConsumerK(const ConsumerK &other) : raft::Kernel(), nfanin(other.nfanin) {
    for (int i = 0; nfanin > i; ++i) {
      add_input<MsgPtr>(port_name_of_i(i));
    }
  }

  CLONE();

  virtual raft::kstatus run() {
    for (auto &port : (this)->input) {
      if (0 == port.size()) {
        continue;
      }
      MsgPtr msg;
      port.pop(msg);
      process(msg.msg);
    }
    return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
    MsgPtr msg;
    dataIn.pop(msg);
    process(msg.msg);
    return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

private:

  void process(double *pmsg) {
    double buf[msgSz / sizeof(double)];
    memcpy((void*)buf, (void*)pmsg, msgSz);
    if (sleep_nsec) {
      nopcompute(sleep_nsec);
    }
  }

  const int nfanin;
};


int main(int argc, char* argv[]) {
  int i;
  pex = pey = 2; /* default values */
  repeats = 7;
  msgSz = 7 * sizeof(double);
  sleep_nsec = 1000;
  sleep_nsec2 = 1000;
  burst_amp = 0;
  fast_period = 1;
  slow_period = 1;
  for (i = 0; argc > i; ++i) {
    if (0 == strcmp("-pex", argv[i])) {
      pex = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-pey", argv[i])) {
      pey = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-iterations", argv[i])) {
      repeats = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-sleep2", argv[i])) {
      sleep_nsec2 = atol(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-sleep", argv[i])) {
      sleep_nsec = atol(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-msgSz", argv[i])) {
      msgSz = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-burst_amp", argv[i])) {
      burst_amp = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-fast_period", argv[i])) {
      fast_period = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-slow_period", argv[i])) {
      slow_period = atoi(argv[i + 1]);
      ++i;
    }
  }
  printf("Px x Py:        %4d x %4d\n", pex, pey);
  printf("Message Size:         %5d\n", msgSz);
  printf("Iterations:          %5ld\n", repeats);
  nthreads = pex * pey;
  burst_period = slow_period + fast_period;
  printf("slow/fast span: %4ld / %4ld\n", slow_period, fast_period);
  printf("slave sleep:   %4ld +- %4ld\n", sleep_nsec2, burst_amp);
  printf("master sleep:          %4ld\n", sleep_nsec);

  raft::DAG dag;
#ifdef EMBER_INCAST

  ProducerK prod(1);
  ConsumerK cons(nthreads - 1);
#if RAFTLIB_ORIG
  repeats /= (nthreads - 1);
  dag += prod >= &cons;
#else
  dag += prod * (nthreads - 1) >> cons;
#endif

#else /* EMBER_OUTCAST */

  ProducerK prod(nthreads - 1);
  ConsumerK cons(1);
#if RAFTLIB_ORIG
  dag += prod <= cons;
#else
  dag += prod >> cons * (nthreads - 1);
#endif

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
