#include <iostream>
#include <vector>
#include <cstdint>
#include <cstdlib>
#include <chrono>

#include <raft>

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#define TAPS_FIR 16

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

typedef double data_t;


class FIR{
public:
    FIR(data_t *coefficients, unsigned int number_of_taps);
    ~FIR();
    data_t filter(data_t input);

private:
    data_t        *coeffs;
    data_t        *buffer;
    unsigned int   taps;
    unsigned int   offset = 0;
};

FIR::FIR(data_t *coefficients, unsigned int number_of_taps):
    coeffs(new data_t[number_of_taps]),
    buffer(new data_t[number_of_taps]),
    taps(number_of_taps)
{
    for(unsigned int i=0;i<number_of_taps;i++) {
        coeffs[i] = coefficients[i];
        buffer[i] = 0;
    }
}

FIR::~FIR()
{
    delete[] buffer;
    delete[] coeffs;
}

data_t FIR::filter(data_t input)
{
    data_t *pcoeffs     = coeffs;
    const data_t *coeffs_end = pcoeffs + taps;

    data_t *buf_val = buffer + offset;

    *buf_val = input;
    data_t output = 0;

    while(buf_val >= buffer){
        output += (*buf_val) * (*pcoeffs);
        buf_val--;
        pcoeffs++;
    }

    buf_val = buffer + taps-1;

    while(pcoeffs < coeffs_end){
        output += (*buf_val) * (*pcoeffs);
        buf_val--;
        pcoeffs++;
    }

    offset++;
    if(offset >= taps) offset = 0;

    return output;
}


class input_stream : public raft::Kernel {
public:
    input_stream(const std::size_t num_samples) :
        raft::Kernel(), nsamples(num_samples) {
        add_output<data_t>("0"_port);
        srand(nsamples);
    }

#if RAFTLIB_ORIG
    virtual raft::kstatus run() { /* } */
        output["0"_port].push((data_t)(rand() % 1000));
#else
    virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                           raft::StreamingData &bufOut) {
        bufOut.push((data_t)(rand() % 1000));
#endif
        if (nsamples < ++cnt) {
            return raft::kstatus::stop;
        }
        return raft::kstatus::proceed;
    }
#if ! RAFTLIB_ORIG
    virtual bool pop(raft::Task *task, bool dryrun) { return true; }
    virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

private:
    const std::size_t nsamples;
    std::size_t cnt = 0;
};

class queued_fir : public raft::Kernel {
public:
  queued_fir(data_t *coeffs) :
      raft::Kernel(), fir(coeffs, TAPS_FIR) {
      add_input<data_t>("0"_port);
      add_output<data_t>("0"_port);
  }

#if RAFTLIB_ORIG
  virtual raft::kstatus run() {
      data_t input_data;
      input["0"_port].pop(input_data);
      output["0"_port].push(fir.filter(input_data));
      return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
      data_t input_data;
      dataIn.pop(input_data);
      bufOut.push(fir.filter(input_data));
      return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

private:
  FIR fir;
};


class output_stream : public raft::Kernel {
public:
  output_stream() :
      raft::Kernel() {
      add_input<data_t>("0"_port);
      cnt = 0;
  }

#if RAFTLIB_ORIG
  virtual raft::kstatus run() {
      data_t input_data;
      input["0"_port].pop(input_data);
      cnt++;
      return raft::kstatus::proceed;
  }
#else
  virtual raft::kstatus::value_t compute(raft::StreamingData &dataIn,
                                         raft::StreamingData &bufOut) {
      data_t input_data;
      dataIn.pop(input_data);
      cnt++;
      return raft::kstatus::proceed;
  }
  virtual bool pop(raft::Task *task, bool dryrun) { return true; }
  virtual bool allocate(raft::Task *task, bool dryrun) { return true; }
#endif

  std::size_t cnt;
};


int main(int argc, char **argv) {
    unsigned int stages  = 32;
    unsigned int samples = 100;

    if(2 < argc) {
        samples = atoll( argv[2] );
    }
    if(1 < argc) {
        stages = atoll( argv[1] );
        if(2 >= stages) {
            std::cout << "Needs at least 3 stages\n";
            return 1;
        }
    }
    std::cout << argv[0] << " FIR stages = " << stages <<
        ", samples = " << samples << "\n" ;

    data_t coeffs[TAPS_FIR] =  {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
                                0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1};

    input_stream istream(samples);
    queued_fir fir(coeffs);
    output_stream ostream;

    raft::DAG dag;

#if RAFTLIB_ORIG
    auto kernels( dag += istream >> fir );
    for(unsigned int i = 3; stages > i; ++i) {
        kernels = (dag += kernels.getDst().first->get() >>
                   raft::kernel_maker< queued_fir >(coeffs));
    }
    dag += kernels.getDst().first->get() >> ostream;
#else
    raft::Kpair *kpair = &(istream >> fir);
    for (unsigned int i = 3; stages > i; ++i) {
        kpair = &(*kpair >> raft::kernel_maker<queued_fir>(coeffs));
    }
    dag += *kpair >> ostream;
#endif

    const uint64_t beg_tsc = rdtsc();
    const auto beg( high_resolution_clock::now() );

#if RAFTLIB_ORIG
    dag.exe<
             partition_dummy,
#if USE_UT or USE_QTHREAD
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
             no_parallel >();
#else // not( RAFTLIB_ORIG )
    dag.exe<
#if RAFTLIB_MIX
             raft::RuntimeMix
#elif RAFTLIB_ONESHOT
             raft::RuntimeNewBurst
#elif RAFTLIB_CV
             raft::RuntimeFIFOCV
#else
             raft::RuntimeFIFO
#endif
             >();
#endif

    const uint64_t end_tsc = rdtsc();
    const auto end(high_resolution_clock::now());
    const auto elapsed(duration_cast<nanoseconds>(end - beg));

    std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
    std::cout << elapsed.count() << " ns elapsed\n";
    std::cout << ostream.cnt << std::endl;

    return 0;
}
