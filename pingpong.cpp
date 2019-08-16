#include <iostream>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <thread>
#include <chrono>
#include <array>
#include <time.h>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include <cstdlib>

#include "affinity.hpp"

#include <boost/lockfree/queue.hpp>

#ifdef VL
#include <vl.h>
#endif

#ifdef GEM5
#include "gem5/m5ops.h"
#endif


#define CAPACITY 4096

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
  bool push(ball_t ball) { double_vl_push_strong(&in, ball.val); return true; }
  bool pop(ball_t &ball) {
    bool valid;
    double_vl_pop_non(&out, &ball.val, &valid);
    return valid;
  }
  void open(int fd, int num_cachelines = 1) {
    open_double_vl_as_producer(fd, &in, num_cachelines);
    open_double_vl_as_consumer(fd, &out, num_cachelines);
  }
  void close() {
    close_double_vl_as_producer(in);
    close_double_vl_as_consumer(out);
  }
  ~vl_q_t() { close(); }
};

vl_q_t mosi_vl,
       miso_vl;

#endif

/**
 * used to act as a marker flag for when all
 * threads are ready, actual declaration is
 * in main.
 */
using atomic_t = std::atomic< int >;


struct alignas( 64 ) /** align to 64B boundary **/ playerArgs
{
    std::uint64_t       burst;
    std::uint64_t       round;
#ifdef VL
    vl_q_t              *qmosi  = nullptr;
    vl_q_t              *qmiso  = nullptr;
#else
    boost_q_t           *qmosi  = nullptr;
    boost_q_t           *qmiso  = nullptr;
#endif
};

void
ping( playerArgs const * const pargs, atomic_t &ready )
{
    affinity::set( 0 );

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

    affinity::set( 1 );

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
    std::cout << "VL queues opened\n";
#endif
#endif /** end initiation of VL **/

    atomic_t    ready( -1 );

    playerArgs args[2];

    args[0].burst   = burst;
    args[0].round   = round;
#ifdef VL
    args[0].qmosi   = &mosi_vl;
    args[0].qmiso   = &miso_vl;
#else
    args[0].qmosi   = &mosi_boost;
    args[0].qmiso   = &miso_boost;
#endif
    args[1].burst   = burst;
    args[1].round   = round;
#ifdef VL
    args[1].qmosi   = &mosi_vl;
    args[1].qmiso   = &miso_vl;
#else
    args[1].qmosi   = &mosi_boost;
    args[1].qmiso   = &miso_boost;
#endif

    std::thread playerm( ping, &args[0], std::ref( ready ) );
    std::thread players( pong, &args[1], std::ref( ready ) );

    const auto start( std::chrono::high_resolution_clock::now() );

#ifdef GEM5
    m5_reset_stats(0, 0);
#endif

    ready++;

    playerm.join();
    players.join();

#ifdef GEM5
    m5_dump_reset_stats(0, 0);
#endif

    const auto end( std::chrono::high_resolution_clock::now() );
    const auto elapsed(
        std::chrono::duration_cast< std::chrono::nanoseconds >( end - start )
        );

    std::cout << elapsed.count() << "ns elapsed\n";
    std::cout << elapsed.count() / round << "ns average per round(" <<
      burst << " pushs " << burst << " pops)\n";

#ifdef VL
    // TODO: rmvl();
#ifdef VERBOSE
    std::cout << "VL released\n";
#endif
#endif
    return( EXIT_SUCCESS );
}
