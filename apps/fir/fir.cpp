#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <cstdlib>
#include <boost/lockfree/queue.hpp>

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

#ifdef VL
#include "vl/vl.h"
#endif

#ifdef ZMQ
#include <assert.h>
#include <zmq.h>
#endif

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#define NUM_CORES 4 
#define CAPACITY 4096
#define TAPS_FIR 16

#ifndef STDATOMIC
using atomic_t = boost::atomic< unsigned int >;
#else
using atomic_t = std::atomic< unsigned int >;
#endif

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

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
    bool push(data_t data) {
        uint64_t *tp = (uint64_t*) &data;
        twin_vl_push_strong(&in, *tp);
        return true;
    }
    void flush(){
        twin_vl_flush(&in);
    }
    bool pop(data_t &data) {
        uint64_t temp;
        twin_vl_pop_strong(&out, &temp);
        data_t *tp = (data_t*) &temp;
        data = *tp;
        return true;
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
  bool push(data_t data) {
    assert(sizeof(data) == zmq_send(out, &data, sizeof(data), 0));
    return true;
  }
  bool pop(data_t &data) {
    bool valid = false;
    if (0 < zmq_recv(in, &data, sizeof(data), ZMQ_DONTWAIT)) {
      valid = true;
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


void
input_stream(
#ifdef VL
    int fd,
#elif ZMQ
    zmq_q_t* q_out,
#else 
    boost_q_t* q_out,
#endif
    unsigned int samples,
    atomic_t &ready,
    unsigned int num_threads
){
    setAffinity( (ready+1)%NUM_CORES );
    //std::cout << "Input : " << (ready+1)%NUM_CORES << std::endl;
#ifdef VL
    vl_q_t q_out;
    q_out.open(fd, 1, true);
    vl_q_t *out = &q_out;
#elif ZMQ
    q_out.open("in_stream", true);
    zmq_q_t *out = q_out;
#else
    boost_q_t *out = q_out;
#endif
    unsigned int t_samples(samples);
    srand (256);
    ready++;
    while( ready != num_threads ){ /** spin **/ };
    while(t_samples--)
    {
        data_t input_data = (data_t)(rand() % 1000);
        while(!out->push(input_data));
    }
#ifdef VL
    out->flush();
#endif
    return; 
}

void
queued_fir(
#ifdef VL
    int fd_in,
    int fd_out,
#elif ZMQ
    zmq_q_t* q_in,
    zmq_q_t* q_out,
#else 
    boost_q_t* q_in,
    boost_q_t* q_out,
#endif
    unsigned int samples,
    atomic_t &ready,
    unsigned int num_threads
){
    setAffinity( (ready+1)%NUM_CORES );
    //std::cout << "FIR : " << (ready+1)%NUM_CORES << std::endl;
#ifdef VL
    vl_q_t q_in, q_out;
    q_in.open(fd_in, 1, false);
    q_out.open(fd_out, 1, true);
    vl_q_t *in = &q_in;
    vl_q_t *out = &q_out;
#elif ZMQ
    q_in->open("unique text", false);
    zmq_q_t *in = q_in;
    q_out->open("ddydhkdjeiy", true);
    zmq_q_t *out = q_out;
#else
    boost_q_t *in  = q_in;
    boost_q_t *out = q_out;
#endif

    unsigned int t_samples(samples);

    data_t coeffs[TAPS_FIR] =  {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
                                0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1};

    FIR *fir1= new FIR(coeffs, TAPS_FIR);
    data_t input_data;
    ready++;
    while( ready != num_threads ){ /** spin **/ };
    while(t_samples--)
    {
        while(!in->pop(input_data));
        while(!out->push(fir1->filter(input_data)));
    }
#ifdef VL
    out->flush();
#endif
    delete fir1;
    return; 
}


void
output_stream(
#ifdef VL
    int fd,
#elif ZMQ
    zmq_q_t* q_in,
#else 
    boost_q_t* q_in,
#endif
    unsigned int samples,
    atomic_t &ready,
    unsigned int num_threads
){
    setAffinity( (ready+1)%NUM_CORES );
    //std::cout << "Output : " << (ready+1)%NUM_CORES << std::endl;
#ifdef VL
    vl_q_t q_in;
    q_in.open(fd, 1, false);
    vl_q_t *in = &q_in;
#elif ZMQ
    q_in->open("out_stream", false);
    zmq_q_t *in = q_in;
#else
    boost_q_t *in = q_in;
#endif

    unsigned int t_samples(samples);
    data_t output_data;
    ready++;
    while( ready != num_threads ){ /** spin **/ };
    while(t_samples--)
    {
        while(!in->pop(output_data));
	//std::cout << output_data << std::endl;
    }
    return; 
}

int main( int argc, char **argv )
{
    unsigned int stages  = 2;
    unsigned int samples = 100;

    if( 1 < argc )
    {
        stages = atoll( argv[1] );
    }
    if( 2 < argc )
    {
        samples = atoll( argv[2] );
    }
    std::cout << argv[0] << " FIR stages = " << stages << ", samples = " << samples << "\n" ;
#ifdef VL
    int* fds;
    fds = new int [stages+1];
    
    for(int i=0; i <= stages; i++){
        fds[i] = mkvl();
        if (0 > fds[i]) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return fds[i];
        }
    }
#ifdef VERBOSE
    std::cout << "VL queues opened\n";
#endif
#elif ZMQ
    ctx = zmq_ctx_new();
    assert(ctx);
    zmq_q_t* qs;
    qs = new zmq_q_t[stages+1];
#else 
    std::vector<boost_q_t*> qs;
    for (unsigned int i=0; i < stages+1; i++){
        qs.push_back( new boost_q_t( CAPACITY / sizeof(data_t) ) );
    }
#endif

    atomic_t ready(-1);

    thread t_output(
		    output_stream,
#ifdef VL
                    fds[stages],
#else
                    qs[stages],
#endif
                    samples,
                    std::ref(ready),
                    stages+2);

    std::vector<thread> fir_threads;
    for (unsigned int i=0; i < stages; i++){
        fir_threads.push_back(
			      thread(queued_fir,
#ifdef VL
                              fds[i],
                              fds[i+1],
#else 
                              qs[i],
                              qs[i+1],
#endif
                              samples,
                              std::ref(ready),
                              stages+2));
    }

    thread t_input(
		    input_stream,
#ifdef VL
                    fds[0],
#else 
                    qs[0],
#endif
                    samples,
                    std::ref(ready),
                    stages+2);

    std::cout << "On Your Mark! Get Set! Go!\n";
#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif

    ready++;

    t_input.join();
    auto fir_ptr = fir_threads.begin();
    while (fir_ptr != fir_threads.end())
    {
        fir_ptr->join();
        fir_ptr++;
    }
    t_output.join();

#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif
    std::cout << "Good Job Guys !!!\n";
#ifdef VL
    delete[] fds;
#ifdef VERBOSE
    std::cout << "VL released\n";
#endif
#elif ZMQ
    delete[] queues;
#endif
}
