#include <iostream>
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

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#define CAPACITY 4096
#define TAPS_FIR1 16
#define TAPS_FIR2 16
/**
 * used to act as a marker flag for when all
 * threads are ready, actual declaration is
 * in main.
 */
#ifndef STDATOMIC
using atomic_t = boost::atomic< int >;
#else
using atomic_t = std::atomic< int >;
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
int in_fir1_vl_fd;
int fir1_fir2_vl_fd;
int fir2_out_vl_fd;

struct vl_q_t {
    vlendpt_t in;
    vlendpt_t out;
    bool in_assigned = false;
    bool out_assigned = false;
    // TODO: push, pop, open, close should use VL functions after
    // taking sizeof(data_t)
    bool push(data_t data) {
        uint64_t *tp = (uint64_t*) &data;
        twin_vl_push_strong(&in, *tp);
        //twin_vl_flush(&in);
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
#else
using boost_q_t = boost::lockfree::queue<data_t>;
boost_q_t q_in_fir1   ( CAPACITY / sizeof(data_t) );
boost_q_t q_fir1_fir2 ( CAPACITY / sizeof(data_t) );
boost_q_t q_fir2_out  ( CAPACITY / sizeof(data_t) );
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
input_stream(unsigned int samples, atomic_t &ready)
{
    setAffinity( 0 );
#ifdef VL
    vl_q_t q_in_fir1_vl;
    q_in_fir1_vl.open(in_fir1_vl_fd, 1, true);
    vl_q_t *out = &q_in_fir1_vl;
#else
    boost_q_t *out  = &q_in_fir1;
#endif
    unsigned int t_samples(samples);
    srand (256);
    ready++;
    while( ready != 4 ){ /** spin **/ };
    /** we're ready to get started, both initialized **/
    while(t_samples--)
    {
        /* Input stream - can be replaced with a file read*/
        data_t input_data = (data_t)(rand() % 100);
        while(!out->push(input_data));
    }
#ifdef VL
    out->flush();
#endif
    return; /** end of input_stream function **/
}

void
queued_fir1(unsigned int samples, atomic_t &ready)
{
    setAffinity( 1 );
#ifdef VL
    vl_q_t q_in_fir1_vl, q_fir1_fir2_vl;
    q_in_fir1_vl.open(in_fir1_vl_fd, 1, false);
    q_fir1_fir2_vl.open(fir1_fir2_vl_fd, 1, true);
    vl_q_t *in = &q_in_fir1_vl;
    vl_q_t *out = &q_fir1_fir2_vl;
#else
    boost_q_t *in  = &q_in_fir1;
    boost_q_t *out = &q_fir1_fir2;
#endif

    unsigned int t_samples(samples);

    /* Specify the desired filter here */
    data_t coeffs[TAPS_FIR1] = {0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8,
                                0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1};

    FIR *fir1= new FIR(coeffs, TAPS_FIR1);
    data_t input_data;
    ready++;
    while( ready != 4 ){ /** spin **/ };
    /** we're ready to get started, both initialized **/
    while(t_samples--)
    {
        /* Input stream - can be replaced with a file read*/
        while(!in->pop(input_data));
        while(!out->push(fir1->filter(input_data)));
    }
#ifdef VL
    out->flush();
#endif
    delete fir1;
    return; /** end of input_stream function **/
}

void
queued_fir2(unsigned int samples, atomic_t &ready)
{
    setAffinity( 2 );
#ifdef VL
    vl_q_t q_fir1_fir2_vl, q_fir2_out_vl;
    q_fir1_fir2_vl.open(fir1_fir2_vl_fd, 1, false);
    q_fir2_out_vl.open(fir2_out_vl_fd, 1, true);
    vl_q_t *in = &q_fir1_fir2_vl;
    vl_q_t *out = &q_fir2_out_vl;
#else
    boost_q_t *in  = &q_fir1_fir2;
    boost_q_t *out = &q_fir2_out;
#endif
    unsigned int t_samples(samples);

    /* Specify the desired filter here */
    data_t coeffs[TAPS_FIR2] = {0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1,
                                0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8};

    FIR *fir2= new FIR(coeffs, TAPS_FIR2);
    data_t input_data;
    ready++;
    while( ready != 4 ){ /** spin **/ };
    /** we're ready to get started, both initialized **/
    while(t_samples--)
    {
        /* Input stream - can be replaced with a file read*/
        while(!in->pop(input_data));
        while(!out->push(fir2->filter(input_data)));
    }
#ifdef VL
    out->flush();
#endif
    delete fir2;
    return; /** end of input_stream function **/
}


void
output_stream(unsigned int samples, atomic_t &ready)
{
    setAffinity( 3 );
#ifdef VL
    vl_q_t q_fir2_out_vl;
    q_fir2_out_vl.open(fir2_out_vl_fd, 1, false);
    vl_q_t *in = &q_fir2_out_vl;
#else
    boost_q_t *in = &q_fir2_out;
#endif

    unsigned int t_samples(samples);
    data_t output_data;
    ready++;
    while( ready != 4 ){ /** spin **/ };
    /** we're ready to get started, both initialized **/
    while(t_samples--)
    {
        /* Output stream - can be replaced with a file write*/
        while(!in->pop(output_data));
    }
    return; /** end of output_stream function **/
}

int main( int argc, char **argv )
{
    unsigned int samples = 1000;

    if( 1 < argc )
    {
        samples = atoll( argv[1] );
    }
    std::cout << argv[0] << " samples=" << samples << "\n";

#ifdef VL
    in_fir1_vl_fd = mkvl();
    if (0 > in_fir1_vl_fd) {
        std::cerr << "mkvl() return invalid file descriptor\n";
        return in_fir1_vl_fd;
    }

    fir1_fir2_vl_fd = mkvl();
    if (0 > fir1_fir2_vl_fd) {
        std::cerr << "mkvl() return invalid file descriptor\n";
        return fir1_fir2_vl_fd;
    }

    fir2_out_vl_fd = mkvl();
    if (0 > fir2_out_vl_fd) {
        std::cerr << "mkvl() return invalid file descriptor\n";
        return fir2_out_vl_fd;
    }

#ifdef VERBOSE
    std::cout << "VL queues opened\n";
#endif
#endif

    atomic_t ready(-1);

    thread t_output( output_stream, samples, std::ref(ready));
    thread t_fir2  ( queued_fir2, samples, std::ref(ready));
    thread t_fir1  ( queued_fir1, samples, std::ref(ready));
    thread t_input ( input_stream, samples, std::ref(ready));


    std::cout << "On Your Mark! Get Set! Go!\n";
#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif

    ready++;

    t_input.join();
    t_fir1.join();
    t_fir2.join();
    t_output.join();

#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif
    std::cout << "Good Job Guys !!!\n";
#ifdef VL
    // TODO: rmvl();
#ifdef VERBOSE
    std::cout << "VL released\n";
#endif
#endif
}
