#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <boost/lockfree/queue.hpp>

#include <sched.h>

#ifndef STDATOMIC
#include <boost/atomic.hpp>
#else
#include <atomic>
#endif
#ifndef STDTHREAD
#include <boost/thread.hpp>
#else
#include <thread>
#endif
#ifndef STDCHRONO
#include <boost/chrono.hpp>
#else
#include <chrono>
#endif

#include "threading.h"
#include "timing.h"

#ifdef VLINLINE
#include "vl/vl_inline.h"
#elif VL
#include "vl/vl.h"
#endif

#ifdef ZMQ
#include <assert.h>
#include <zmq.h>
#endif

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#define NUM_CORES 16
#define CAPACITY 4096
#define TAPS_FIR 16

#ifndef STDATOMIC
using atomic_t = boost::atomic< unsigned int >;
#else
using atomic_t = std::atomic< unsigned int >;
#endif

atomic_t ready;

#ifndef STDCHRONO
using boost::chrono::high_resolution_clock;
using boost::chrono::duration_cast;
using boost::chrono::nanoseconds;
#else
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
#endif

typedef double data_t;

#ifdef VL
struct vl_q_t {
    vlendpt_t in;
    vlendpt_t out;
    bool in_assigned = false;
    bool out_assigned = false;
    bool bounded_push(data_t data) {
        uint64_t *tp = (uint64_t*) &data;
        bool valid;
        valid = twin_vl_push_non(&in, *tp);
        return valid;
    }
    void flush(){
        twin_vl_flush(&in);
    }
    bool pop(data_t &data) {
        uint64_t temp;
	bool valid;
        twin_vl_pop_non(&out, &temp, &valid);
	if(valid){
            data_t *tp = (data_t*) &temp;
            data = *tp;
	}
        return valid;
    }
    void open(int fd, int num_cachelines = 1, bool is_producer = true) {
      if(is_producer){
        open_twin_vl_as_producer(fd, &in, num_cachelines);
        in_assigned = true;
      }
      else{
        open_twin_vl_as_consumer(fd, &out, num_cachelines);
        out_assigned = true;
      }
    }

    void close() {
      if(in_assigned) close_twin_vl_as_producer(in);
      if(out_assigned)close_twin_vl_as_consumer(out);
    }
    ~vl_q_t() { close(); }
};

#elif ZMQ
void *ctx;

struct zmq_q_t {
  void *in;
  void *out;
  bool bounded_push(data_t data) {
    bool valid = true;
    if (0 > zmq_send(out, &data, sizeof(data), ZMQ_DONTWAIT)){
        valid = false;
    }
    return valid;
  }
  bool pop(data_t &data) {
    bool valid = true;
    if (0 > zmq_recv(in, &data, sizeof(data), ZMQ_DONTWAIT)) {
        valid = false;
    }
    return valid;
  }
  void open(std::string port, bool isproducer) {
    if (isproducer) {
      out = zmq_socket(ctx, ZMQ_PUSH);
      assert(0 == zmq_bind(out, ("inproc://" + port).c_str()));
    } else {
      in = zmq_socket(ctx, ZMQ_PULL);
      assert(0 == zmq_connect(in, ("inproc://" + port).c_str()));
    }
  }
  void close() {
    assert(0 == zmq_close(in));
    assert(0 == zmq_close(out));
  }
  ~zmq_q_t() { close(); }
};
#else
using boost_q_t = boost::lockfree::queue<data_t>;
#endif

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

unsigned int samples_g;

void*
input_stream(void* args){
#ifdef VL
    vl_q_t* q_out = (vl_q_t*) args;
#elif ZMQ
    zmq_q_t* q_out = (zmq_q_t*) args;
#else
    boost_q_t* q_out = (boost_q_t*) args;
#endif
    unsigned int samples_t = samples_g;
    srand (256);
    ready--;
    while( 0 != ready.load() ){ /** spin **/
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
            nop \n\
            nop \n\
            nop \n\
            "
            :
            :
            :
            );
    };
    while(samples_t--)
    {
        data_t input_data = (data_t)(rand() % 1000);
        while(!q_out->bounded_push(input_data)){
            sched_yield();
        }
    }
#ifdef VL
    q_out->flush();
#endif
    return NULL;
}

struct firArgs {
#ifdef VL
    vl_q_t* q_in;
    vl_q_t* q_out;
#elif ZMQ
    zmq_q_t* q_in;
    zmq_q_t* q_out;
#else
    boost_q_t* q_in;
    boost_q_t* q_out;
#endif
};

void*
queued_fir(void* args){
    firArgs* fir_args = (firArgs*) args;
    unsigned int samples_t = samples_g;

    data_t coeffs[TAPS_FIR] =  {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
                                0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1};

    FIR *fir1= new FIR(coeffs, TAPS_FIR);
    data_t input_data;
    ready--;
    while( 0 != ready.load() ){ /** spin **/
      for (long i = ready.load() + 7; 0 <= i; --i) {
        __asm__ volatile("\
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
    while(samples_t--)
    {
        while(!fir_args->q_in->pop(input_data)){
            sched_yield();
        }
        data_t output_data = fir1->filter(input_data);
        while(!fir_args->q_out->bounded_push(output_data)){
            sched_yield();
        }
    }
#ifdef VL
    fir_args->q_out->flush();
#endif
    delete fir1;
    return NULL;
}

void*
output_stream(void* args){
#ifdef VL
    vl_q_t* q_in = (vl_q_t*) args;
#elif ZMQ
    zmq_q_t* q_in = (zmq_q_t*) args;
#else
    boost_q_t* q_in = (boost_q_t*) args;
#endif
    unsigned int samples_t = samples_g;
    data_t output_data;
    ready--;
    while( 0 != ready.load() ){ /** spin **/
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
    };
    while(samples_t--)
    {
        while(!q_in->pop(output_data)){
            sched_yield();
        }
    }
    return NULL;
}

int main( int argc, char **argv )
{
    unsigned int aff     = 1;
    unsigned int stages  = 2;
    unsigned int samples = 100;
    char core_list[] = "1-3";

    if( 3 < argc )
    {
        parseCoreList(argv[3]);
    } else
    {
        parseCoreList(core_list);
    }
    pinAtCoreFromList(0);
    if( 2 < argc )
    {
        samples = atoll( argv[2] );
    }
    if( 1 < argc )
    {
        stages = atoll( argv[1] );
    }
    std::cout << argv[0] << " FIR stages = " << stages <<
        ", samples = " << samples << "\n" ;
    ready = stages + 3; // stages of fir_threads and 1 output, 1 input and this
    samples_g = samples;
#ifdef VL
    int* fds;
    fds  = new int [stages+1];
    vl_q_t* p_qs;
    vl_q_t* c_qs;
    p_qs = new vl_q_t [stages+1];
    c_qs = new vl_q_t [stages+1];

    for(unsigned int i=0; i <= stages; i++){
        fds[i] = mkvl();
        if (0 > fds[i]) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return fds[i];
        }
        p_qs[i].open(fds[i], 1, true);
        c_qs[i].open(fds[i], 1, false);
    }
#elif ZMQ
    ctx = zmq_ctx_new();
    assert(ctx);
    std::vector<zmq_q_t*> qs;
    for (unsigned int i=0; i < stages+1; i++){
	const auto val = std::to_string(i);
	const auto q   = new zmq_q_t();
        q->open(val, false);
        q->open(val, true);
        qs.push_back(q);
    }
#else
    std::vector<boost_q_t*> qs;
    for (unsigned int i=0; i < stages+1; i++){
        const auto q = new boost_q_t( CAPACITY / sizeof(data_t) );
        qs.push_back(q);
    }
#endif

    pthread_t t_output;
    threadCreate(&t_output, NULL, output_stream,
#ifdef VL
                 (void*)&c_qs[stages],
#else
                 (void*)qs[stages],
#endif
                 aff++
                );

    pthread_t fir_threads[stages];
    firArgs fir_args[stages];
    for (unsigned int i=0; i < stages; i++){
        fir_args[i].q_in =
#ifdef VL
                     &c_qs[i];
#else
                     qs[i];
#endif
        fir_args[i].q_out =
#ifdef VL
                     &p_qs[i+1];
#else
                     qs[i+1];
#endif

        threadCreate(&fir_threads[i], NULL, queued_fir,
                     (void*)&fir_args[i], aff++);
    }

    pthread_t t_input;
    threadCreate(&t_input, NULL, input_stream,
#ifdef VL
                 (void*)&p_qs[0],
#else
                 (void*)qs[0],
#endif
                 aff++
                );

    while (1 != ready.load()) { /* spin */
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
#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif

    ready--;
    std::cout << "GO\n";

    pthread_join(t_input, NULL);
    for (unsigned int i=0; i < stages; i++)
    {
        pthread_join(fir_threads[i], NULL);
    }
    pthread_join(t_output, NULL);

#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif
    std::cout << "Good Job Guys !!!\n";
#ifdef VL
    delete[] fds;
    delete[] p_qs;
    delete[] c_qs;
#endif
}
