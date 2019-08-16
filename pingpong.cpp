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

#ifdef VL
#include <vl.h>
#else
#include <boost/lockfree/queue.hpp>
#endif

#ifdef GEM5
#include "gem5/m5ops.h"
#endif


#define CAPACITY 4096

using ball_t = std::uint64_t;

#ifdef VL
int         left_vl_fd;
int         right_vl_fd;

vlendpt_t   left_prod_vl, 
            left_cons_vl, 
            right_prod_vl, 
            right_cons_vl;

#else /** not defined VL **/
using boost_q_t = boost::lockfree::queue< ball_t >;
boost_q_t left_lockfree  ( CAPACITY / sizeof(ball_t) );
boost_q_t right_lockfree ( CAPACITY / sizeof(ball_t) );
#endif

/** 
 * used to act as a marker flag for when all 
 * threads are ready, actual declaration is 
 * in main. 
 */
using atomic_t = std::atomic< int >;


struct alignas( 64 ) /** align to 64B boundary **/ playerArgs 
{
    std::uint64_t       round;
#ifdef VL

#else
    boost_q_t           *ql     = nullptr;
    boost_q_t           *qr     = nullptr;
#endif
    char padd[ 4096 ]; /** make darned sure these aren't on the same cache line **/
};

void
player1( playerArgs const *pargs, atomic_t &ready ) 
{
    affinity::set( 0 );

    auto round( pargs->round  );
    
    auto * const psend( pargs->ql );
    auto * const precv( pargs->qr );
    
    ball_t  ball( 0 );

    /** we're ready to start **/
    ready++;
    
    while( ready != 2 ){ /** spin **/ };
    
    
    /** we're ready to get started, both initialized **/
    
    while( round--  )
    {
#if DEBUG                
        std::cout << (left ? "L" : "R") << " @ CPU " << sched_getcpu() << "\n";
#endif
        while( ! psend->push( ball++ ) );
        ball_t receipt;
        while( ! precv->pop( receipt ) );
    } 
    return; /** end of player function **/
}

void
player2( playerArgs const *pargs, atomic_t &ready ) 
{

    auto round( pargs->round  );
    
    affinity::set( 24  );
    
    auto *psend( pargs->qr );
    auto *precv( pargs->ql );
    
    /** we're ready to start **/
    ready++;
    
    while( ready != 2 ){ /** spin **/ };
    
    
    /** we're ready to get started, both initialized **/
    
    while( round--  )
    {
        ball_t ball;
        while( ! precv->pop( ball ) );
        while( ! psend->push( ball ) );
    } 
    return; /** end of player function **/
}

int main( int argc, char **argv ) 
{
    uint64_t round = 10;
    
    if( argc == 2 ) 
    {
        round = atoll( argv[1] );
    }
    else
    {
        std::cerr << "usage is: " << argv[0] << " <#rounds>\n";
        exit( EXIT_FAILURE );
    }

#ifdef VL
        left_vl_fd = mkvl();
        if (0 > left_vl_fd) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return left_vl_fd;
        }
        open_double_vl_as_producer(left_vl_fd, &left_prod_vl, 1);
        open_double_vl_as_consumer(left_vl_fd, &left_cons_vl, 1);
        right_vl_fd = mkvl();
        if (0 > right_vl_fd) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return right_vl_fd;
        }
        open_double_vl_as_producer(right_vl_fd, &right_prod_vl, 1);
        open_double_vl_as_consumer(right_vl_fd, &right_cons_vl, 1);
#endif /** end initiation of VL **/

    atomic_t    ready( -1 );

    playerArgs args[2];

    args[0].round   = round;
#ifdef VL

#else
    args[0].ql      = &left_lockfree;
    args[0].qr      = &right_lockfree;
#endif
    args[1].round   = round;
#ifdef VL

#else
    args[1].ql      = &left_lockfree;
    args[1].qr      = &right_lockfree;
#endif



    std::thread playerl( player1, &args[0], std::ref( ready ) );
    std::thread playerr( player2, &args[1], std::ref( ready ) );
    
    const auto start( std::chrono::high_resolution_clock::now() );

#ifdef GEM5
    m5_reset_stats(0, 0);
#endif

    ready++;

    playerl.join();
    playerr.join();

#ifdef GEM5
    m5_dump_reset_stats(0, 0);
#endif

    const auto end( std::chrono::high_resolution_clock::now() );
    const auto time_duration( std::chrono::duration_cast< std::chrono::nanoseconds >( end - start ) ); 
    
    std::cout << time_duration.count() << "ns elapsed\n";
    std::cout << time_duration.count() / round /** #rounds **/ / 2 /** #queue trips per round **/ << "ns average per trip\n";
        
#ifdef VL        
    close_double_vl_as_producer( left_prod_vl   );
    close_double_vl_as_consumer( left_cons_vl   );
    close_double_vl_as_producer( right_prod_vl  );
    close_double_vl_as_consumer( right_cons_vl  );
#endif
    return( EXIT_SUCCESS );
}
