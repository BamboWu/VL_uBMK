#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <string>
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

#ifdef CAF
#include "caf.h"
#endif

#ifdef ZMQ
#include <assert.h>
#include <zmq.h>
#endif

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#define CAPACITY 4096

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

using ball_t = union {
  std::uint64_t val;
  std::uint8_t arr[8];
};

using boost_q_t = boost::lockfree::queue< ball_t >;

boost_q_t mosi_boost  ( CAPACITY / sizeof(ball_t) );
boost_q_t miso_boost ( CAPACITY / sizeof(ball_t) );

#ifdef VL
int mosi_vl_fd,
    miso_vl_fd;

struct vl_q_t {
  vlendpt_t in;
  vlendpt_t out;
  bool push(ball_t ball) { twin_vl_push_strong(&in, ball.val); return true; }
  bool pop(ball_t &ball) {
    bool valid;
    twin_vl_pop_non(&out, &ball.val, &valid);
    return valid;
  }
  void open(int fd, int num_cachelines = 1) {
    open_twin_vl_as_producer(fd, &in, num_cachelines);
    open_twin_vl_as_consumer(fd, &out, num_cachelines);
  }
  void close() {
    close_twin_vl_as_producer(in);
    close_twin_vl_as_consumer(out);
  }
  ~vl_q_t() { close(); }
};

vl_q_t mosi_vl,
       miso_vl;

#endif

#ifdef CAF
#define MOSI_QID 0
#define MISO_QID 1

struct caf_q_t {
  cafendpt_t in;
  cafendpt_t out;
  bool push(ball_t ball) { caf_push_strong(&in, ball.val); return true; }
  bool pop(ball_t &ball) { return caf_pop_non(&out, &ball.val); }
  void open(int qid) {
      open_caf(qid, &in);
      open_caf(qid, &out);
  }
  void close() {
      close_caf(in);
      close_caf(out);
  }
  ~caf_q_t() { close(); }
};

caf_q_t mosi_caf;
caf_q_t miso_caf;
#endif

#ifdef ZMQ
void *ctx;

struct zmq_q_t {
  void *in;
  void *out;
  bool push(ball_t ball) {
    assert(sizeof(ball) == zmq_send(out, &ball, sizeof(ball), 0));
    return true;
  }
  bool pop(ball_t &ball) {
    bool valid = false;
    if (0 < zmq_recv(in, &ball, sizeof(ball), ZMQ_DONTWAIT)) {
      valid = true;
    }
    return valid;
  }
  void open(std::string port) {
    in = zmq_socket(ctx, ZMQ_PULL);
    out = zmq_socket(ctx, ZMQ_PUSH);
    assert(0 == zmq_bind(out, ("inproc://" + port).c_str()));
    assert(0 == zmq_connect(in, ("inproc://" + port).c_str()));
  }
  void close() {
    assert(0 == zmq_close(in));
    assert(0 == zmq_close(out));
  }
  ~zmq_q_t() { close(); }
};

zmq_q_t mosi_zmq,
        miso_zmq;
#endif

#ifdef M5VL
struct m5_q_t {
  int vlink_id = 0;
  std::uint8_t __attribute__((aligned(64))) prod_line[64];
  std::uint8_t __attribute__((aligned(64))) cons_line[64];
  bool push(ball_t ball) {
    uint8_t Ptr = prod_line[62] & 0x3f;
    if (48 < Ptr) { /* no empty space left */
      m5_vl_push((uint64_t)prod_line, vlink_id); /* always succeed */
      prod_line[62] = 0xf0; /* Ptr = 0x30 = 48 */
      Ptr = prod_line[62] & 0x3f;
    }
    uint64_t *pval64 = (uint64_t*) &prod_line[Ptr];
    *pval64 = ball.val;
    Ptr -= 8;
    if (48 < Ptr) { /* filled up */
      m5_vl_push((uint64_t)prod_line, vlink_id);
      prod_line[62] = 0xf0; /* Ptr = 0x30 = 48 */
      Ptr = prod_line[62] & 0x3f;
    }
    prod_line[62] = (prod_line[62] & 0xc0) | (Ptr & 0x3f);
    return true;
  }
  bool pop(ball_t &ball) {
    uint8_t Ptr = cons_line[62] & 0x3f;
    bool isvalid = false;
    if (48 >= Ptr) { /* has valid data */
      uint64_t *pval64 = (uint64_t*) &cons_line[Ptr];
      ball.val = *pval64;
      Ptr -= 8;
      isvalid = true;
    }
    if (48 < Ptr) { /* empty */
      m5_vl_pop((uint64_t)cons_line, vlink_id);
    }
    cons_line[62] = (cons_line[62] & 0xc0) | ((Ptr - 8) & 0x3f);
    return isvalid;
  }
  void open(int fd) {
    vlink_id = fd;
    prod_line[63] = cons_line[63] = 0;
    prod_line[62] = 0xf0; /* Ptr = 0x30 = 48 */
    cons_line[62] = 0xf8; /* Ptr = 0x38 = 56, underflow */
  }
  void close() {
    uint8_t Ptr = prod_line[62] & 0x3f;
    if (((Ptr + 8) & 0x3f) < 0x38) { /* prod_line has data left */
      m5_vl_push((uint64_t)prod_line, vlink_id);
    }
    Ptr = cons_line[62] & 0x3f;
    int num_valid = (Ptr + 8) < 0x38 ? (Ptr + 8) : 0;
    if (num_valid) {
      printf("TODO: transform cons_line to prod_line and push\n");
    }
  }
};

m5_q_t mosi_m5,
       miso_m5;
#endif

struct alignas( 64 ) /** align to 64B boundary **/ playerArgs
{
    std::uint64_t       burst;
    std::uint64_t       round;
#ifdef VL
    vl_q_t              *qmosi  = nullptr;
    vl_q_t              *qmiso  = nullptr;
#elif M5VL
    m5_q_t              *qmosi  = nullptr;
    m5_q_t              *qmiso  = nullptr;
#elif CAF
    caf_q_t             *qmosi  = nullptr;
    caf_q_t             *qmiso  = nullptr;
#elif ZMQ
    zmq_q_t             *qmosi  = nullptr;
    zmq_q_t             *qmiso  = nullptr;
#else
    boost_q_t           *qmosi  = nullptr;
    boost_q_t           *qmiso  = nullptr;
#endif
};

void
ping( playerArgs const * const pargs, atomic_t &ready )
{
    setAffinity( 0 );

    auto round( pargs->round );

    std::uint64_t const burst( pargs->burst );

    auto * const psend( pargs->qmosi );
    auto * const precv( pargs->qmiso );

    ball_t  ball = { 0 };
    ball_t  receipt;

    /** we're ready to start **/
    ready++;

    while( ready != 2 ){ /** spin **/ };


    /** we're ready to get started, both initialized **/

    while( round-- )
    {
#if VERBOSE
        std::cout << "M @ CPU " << sched_getcpu() << "\n";
#endif
        for (std::uint64_t i = 0; i < burst; ++i) {
          while( ! psend->push( ball ) );
          ball.val++;
        }
        for (std::uint64_t i = 0; i < burst; ++i) {
          while( ! precv->pop( receipt ) );
#if VERBOSE
          std::cout << (uint64_t)receipt.arr[0] << " " <<
            receipt.val << std::endl;
#endif
        }
        ball.val += 256;
    }
    return; /** end of player function **/
}

void
pong( playerArgs const * const pargs, atomic_t &ready )
{

    setAffinity( 1 );

    auto round( pargs->round );

    std::uint64_t const burst( pargs->burst );

    auto * const psend( pargs->qmiso );
    auto * const precv( pargs->qmosi );
    
    ball_t ball;

    /** we're ready to start **/
    ready++;

    while( ready != 2 ){ /** spin **/ };


    /** we're ready to get started, both initialized **/

    while( round-- )
    {
        for (std::uint64_t i = 0; i < burst; ++i) {
          while( ! precv->pop( ball ) );
          while( ! psend->push( ball ) );
        }
    }
    return; /** end of player function **/
}

int main( int argc, char **argv )
{
    uint64_t burst = 7;
    uint64_t round = 10;

    if( 2 < argc )
    {
        burst = atoll( argv[2] );
    }
    if( 1 < argc )
    {
        round = atoll( argv[1] );
    }
    std::cout << argv[0] << " round=" << round << " burst=" << burst << "\n";

#ifdef VL
    mosi_vl_fd = mkvl();
    if (0 > mosi_vl_fd) {
        std::cerr << "mkvl() return invalid file descriptor\n";
        return mosi_vl_fd;
    }
    mosi_vl.open(mosi_vl_fd);
    miso_vl_fd = mkvl();
    if (0 > miso_vl_fd) {
        std::cerr << "mkvl() return invalid file descriptor\n";
        return miso_vl_fd;
    }
    miso_vl.open(miso_vl_fd);
#ifdef VERBOSE
    std::cout << "vlinks created\n";
#endif
#elif M5VL
    mosi_m5.open(1);
    miso_m5.open(2);
#elif CAF
    mosi_caf.open(MOSI_QID);
    miso_caf.open(MISO_QID);
#endif /** end initiation of VL **/

#ifdef ZMQ
    ctx = zmq_ctx_new();
    assert(ctx);
    mosi_zmq.open("mosi");
    miso_zmq.open("miso");
#endif

    atomic_t    ready( -1 );

    playerArgs args[2];

    args[0].burst   = burst;
    args[0].round   = round;
#ifdef VL
    args[0].qmosi   = &mosi_vl;
    args[0].qmiso   = &miso_vl;
#elif M5VL
    args[0].qmosi   = &mosi_m5;
    args[0].qmiso   = &miso_m5;
#elif CAF
    args[0].qmosi   = &mosi_caf;
    args[0].qmiso   = &miso_caf;
#elif ZMQ
    args[0].qmosi   = &mosi_zmq;
    args[0].qmiso   = &miso_zmq;
#else
    args[0].qmosi   = &mosi_boost;
    args[0].qmiso   = &miso_boost;
#endif
    args[1].burst   = burst;
    args[1].round   = round;
#ifdef VL
    args[1].qmosi   = &mosi_vl;
    args[1].qmiso   = &miso_vl;
#elif M5VL
    args[1].qmosi   = &mosi_m5;
    args[1].qmiso   = &miso_m5;
#elif CAF
    args[1].qmosi   = &mosi_caf;
    args[1].qmiso   = &miso_caf;
#elif ZMQ
    args[1].qmosi   = &mosi_zmq;
    args[1].qmiso   = &miso_zmq;
#else
    args[1].qmosi   = &mosi_boost;
    args[1].qmiso   = &miso_boost;
#endif

    thread playerm( ping, &args[0], std::ref( ready ) );
    thread players( pong, &args[1], std::ref( ready ) );

    const uint64_t beg_tsc = rdtsc();
    const auto beg( high_resolution_clock::now() );

#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif

    ready++;

    playerm.join();
    players.join();

#ifndef NOGEM5
    m5_dump_reset_stats(0, 0);
#endif

    const uint64_t end_tsc = rdtsc();
    const auto end( high_resolution_clock::now() );
    const auto elapsed( duration_cast< nanoseconds >( end - beg ) );

    std::cout << ( end_tsc - beg_tsc ) << " ticks elapsed\n";
    std::cout << elapsed.count() << " ns elapsed\n";
    std::cout << elapsed.count() / round << " ns average per round (" <<
      burst << " pushs " << burst << " pops)\n";

#ifdef VL
    // TODO: rmvl();
#ifdef VERBOSE
    std::cout << "VL released\n";
#endif
#endif
    return( EXIT_SUCCESS );
}
