/**
 * hint_demo.cpp -
 * @author: Qinzhe Wu
 * @version: Sat May 27 11:20:03 2023
 *
 * Copyright 2023 The Regents of the University of Texas
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <raft>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#define MAX_ARR_LEN 1024

class RandGen : public raft::Kernel
{
public:
    RandGen( const std::size_t repetitions,
             const std::size_t delay_tick,
             int fire_rate,
             int fanout ) :
        raft::Kernel(),
        nreps( repetitions ),
        delay( delay_tick ),
        nfanouts( fanout )
    {
#if RAFTLIB_ORIG
        for( auto i( 0 ); fanout > i; ++i )
        {
            add_output< std::size_t >( port_name_of_i( i ) );
        }
#else
        add_output< std::size_t >( "0"_port );
#endif
        //const std::size_t nbuckets = MAX_ARR_LEN / nfanouts;
        //arr_len = nbuckets * nfanouts;
        arr_len = ( MAX_ARR_LEN / nfanouts ) * nfanouts;
        //std::vector< int > fire_buckets( nbuckets, 0 );
        std::vector< int > fire_buckets( arr_len, 0 );
        srand( fire_rate );
        //for( std::size_t i( 0 ); nbuckets * fire_rate * nfanouts > i; ++i )
        //{
        //    fire_buckets[ rand() % nbuckets ]++;
        //}
        for( std::size_t i( 0 ); arr_len > i; ++i )
        {
            arr[ i ] = 0;
        }
        for( std::size_t i( 0 ); arr_len * fire_rate > i; ++i )
        {
            arr[ rand() % arr_len ]++;
        }
        //for( std::size_t i( 0 ); nbuckets > i; ++i )
        //{
        //    int offset = i % nfanouts;
        //    while( 0 < fire_buckets[ i ]-- )
        //    {
        //        arr[ i * nfanouts + offset ]++;
        //        offset = ( offset + 1 ) % nfanouts;
        //    }
        //}
        //for( std::size_t i( 0 ); arr_len > i; ++i )
        //{
        //    std::cout << arr[ i ] << " ";
        //    if( ( nfanouts - 1 ) == ( i % nfanouts ) )
        //    {
        //        std::cout << std::endl;
        //    }
        //}
        rep = 0;
        cnt = 0;
    }

    virtual ~RandGen() = default;

#if RAFTLIB_ORIG
    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        for( auto &port : (this)->output )
#else
        for( int i( 0 ); nfanouts > i; ++i )
#endif
        {
            for ( int r( 0 ); arr[ rep % arr_len ] > r; ++r )
            {
#if RAFTLIB_ORIG
                port.push( rep );
#else /* RAFTLIB_ORIG */
                bufOut.push( rep );
#endif
            }
            cnt += arr[ rep % arr_len ];
            auto beg_tick( rdtsc() );
            while( delay > ( rdtsc() - beg_tick ) )
            {
                /* busy doing nothing until delay reached */
            }
            if( nreps < ++rep )
            {
                return( raft::kstatus::stop );
            }
        }
        return( raft::kstatus::proceed );
    }

#if RAFTLIB_ORIG
#else
    virtual bool pop( raft::Task *task, bool dryrun )
    {
        return true;
    }

    virtual bool allocate( raft::Task *task, bool dryrun )
    {
        return true;
    }
#endif
    std::size_t cnt;

private:
    const std::size_t nreps;
    const std::size_t delay;
    std::size_t rep;
    int nfanouts;
    std::size_t arr_len;
    int arr[ MAX_ARR_LEN ];
};

class SinkKernel : public raft::Kernel
{
public:
    SinkKernel( const std::size_t delay_tick ) :
        raft::Kernel(),
        delay( delay_tick )
    {
        add_input< std::size_t >( "0"_port );
    }

    SinkKernel( const SinkKernel &other ) :
        raft::Kernel(),
        delay( other.delay )
    {
        add_input< std::size_t >( "0"_port );
    }

#if RAFTLIB_ORIG

    CLONE(); /* enable cloning */

    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
        std::size_t rep;
#if RAFTLIB_ORIG
        input[ "0"_port ].pop( rep );
#else /* RAFTLIB_ORIG */
        dataIn.pop( rep );
#endif
        auto beg_tick( rdtsc() );
        while( delay > ( rdtsc() - beg_tick ) )
        {
            /* busy doing nothing until delay reached */
        }
        return( raft::kstatus::proceed );
    }

#if RAFTLIB_ORIG
#else
    virtual bool pop( raft::Task *task, bool dryrun )
    {
        return true;
    }

    virtual bool allocate( raft::Task *task, bool dryrun )
    {
        return true;
    }
#endif

private:
    const std::size_t delay;
};


int
main( int argc, char **argv )
{
    std::size_t repetitions = 1000;
    std::size_t delay_tick = 200;
    std::size_t offset_tick = 60;
    int fire_rate = 2;
    int fanout = 1;

    if( argc < 2 )
    {
        std::cerr << "Usage: ./" << argv[0] <<
            " <orig|pw|cv|os|mix> [#repetitions=" << repetitions <<
            "] [delay_tick=" << delay_tick <<
            "] [offset_tick=" << offset_tick <<
            "] [fire_rate=" << fire_rate <<
            "] [fanout=" << fanout << "]\n";
        exit( EXIT_FAILURE );
    }
    else if( 2 < argc )
    {
        repetitions = atol( argv[ 2 ] );
        if( 3 < argc )
        {
            delay_tick = atol( argv[ 3 ] );
            if( 4 < argc )
            {
                offset_tick = atol( argv[ 4 ] );
            }
            if( 5 < argc )
            {
                fire_rate = atoi( argv[ 5 ] );
            }
            if( 6 < argc )
            {
                fanout = atoi( argv[ 6 ] );
            }
        }
    }

    std::string runtime_choice( argv[ 1 ] );
#if RAFTLIB_ORIG
    if( "orig" != runtime_choice )
#else
    if( "orig" == runtime_choice )
#endif
    {
        exit( EXIT_FAILURE );
    }

    std::cout << runtime_choice << " #repetitions=" << repetitions <<
        " delay_tick=" << delay_tick << " offset_tick=" << offset_tick <<
        " fire_rate=" << fire_rate << " fanout=" << fanout << std::endl;

    RandGen rg( repetitions, delay_tick, fire_rate, fanout );
    SinkKernel cons( delay_tick + offset_tick );

    raft::DAG dag;

#if RAFTLIB_ORIG
    dag += rg <= cons;
#else
    dag += rg >> cons * fanout;
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
    if( "mixcv" == runtime_choice )
    {
        dag.exe< raft::RuntimeGroupMixCV >();
    }
    else if( "cv" == runtime_choice )
    {
        dag.exe< raft::RuntimeGroupFIFOCV >();
    }
    else if ( "pw" == runtime_choice )
    {
        dag.exe< raft::RuntimeGroupFIFO >();
    }
    else if ( "os" == runtime_choice )
    {
        dag.exe< raft::RuntimeGroupNewBurst >();
    }
    else
    {
        dag.exe< raft::RuntimeGroupMix >();
    }
#endif

    std::cout << "cnt=" << rg.cnt << std::endl;
    const uint64_t end_tsc = rdtsc();
    const auto end( high_resolution_clock::now() );
    const auto elapsed( duration_cast< nanoseconds >( end - beg ) );
    std::cout << ( end_tsc - beg_tsc ) << " ticks elapsed\n";
    std::cout << elapsed.count() << " ns elapsed\n";
    return( EXIT_SUCCESS );

    return 0;
}
