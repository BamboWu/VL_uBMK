/**
 * armq_demo.cpp -
 * @author: Qinzhe Wu
 * @version: Wed Apr 12 17:28:03 2023
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

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "timing.h"

#include "armq_demo.hpp"

std::size_t *Generator::arrs[ NUM_ARRS ];

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

int
main( int argc, char **argv )
{
    int repetitions = 1000;
    int G = 1; /* #generatros/#filters */
    int C = 1; /* #copys */
    std::size_t D = 10; /* delay_tick */
    std::size_t S = 4; /* chunk_size */
    std::size_t rounds = 2; /* number of rounds touching the buffer */
    std::size_t nops = 100; /* number of nops */
    if( argc < 2 )
    {
        std::cerr << "Usage: ./" << argv[0] <<
            " <orig|pw|cv|os|mix> [#repetitions=" << repetitions <<
            "] [G=" << G << "] [C=" << C <<
            "] [D=" << D << "] [S=" << S <<
            "] [#rounds=" << rounds << "][#nops=" << nops << "]\n";
        exit( EXIT_FAILURE );
    }
    else if( 2 < argc )
    {
        repetitions = atoi( argv[ 2 ] );
        if( 3 < argc )
        {
            G = atoi( argv[ 3 ] );
            if( 4 < argc )
            {
                C = atoi( argv[ 4 ] );
                if( 5 < argc )
                {
                    D = strtoull( argv[ 5 ], NULL, 0 );
                }
                if( 6 < argc )
                {
                    S = strtoull( argv[ 6 ], NULL, 0 );
                }
                if( 7 < argc )
                {
                    rounds = strtoull( argv[ 7 ], NULL, 0 );
                }
                if( 8 < argc )
                {
                    nops = strtoull( argv[ 8 ], NULL, 0 );
                }
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
    if( 1 > G || 1 > C )
    {
        exit( EXIT_FAILURE );
    }

#if DUPLICATE_FILTERS
    int F = G;
#else
    int F = 1;
#endif

    std::cout << runtime_choice << " #repetitions=" << repetitions <<
        " G=" << G << " F=" << F << " C=" << C << " D=" << D << " S=" << S <<
        " #rounds=" << rounds << " #nops=" << nops << std::endl;

    Sequencer seq( repetitions, G );
    Generator::populate( S );
    Generator gen( S, D );
    Filter filter( S, rounds, nops, G / F );
    Copy copy( S, F );
    Count count( S );
    raft::DAG dag;

#if RAFTLIB_ORIG
#if DUPLICATE_FILTERS
    auto kernels( dag += seq <= gen >> filter >= copy );
#else
    auto kernels( dag += seq <= gen >= filter >> copy );
#endif
    for( auto i( 1 ); C > i; ++i )
    {
        kernels = ( dag += kernels.getDst().first->get() >>
                    raft::kernel_maker< Copy >( S, 1 ) );
    }
    dag += kernels.getDst().first->get() >> count;
#else
    raft::Kpair *kpair( &( seq >> ( gen * G >> filter * F ) >> copy * 0 ) );
    for( auto i( 1 ); C > i; ++i )
    {
        kpair = &( *kpair >>
                   *raft::kernel_maker< Copy >( S, 1 ) * 0 );
    }
    dag += *kpair >> count * 0;
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

    const uint64_t end_tsc = rdtsc();
    const auto end( high_resolution_clock::now() );
    const auto elapsed( duration_cast< nanoseconds >( end - beg ) );
    std::cout << count.total_cnt << std::endl;
    std::cout << ( end_tsc - beg_tsc ) << " ticks elapsed\n";
    std::cout << elapsed.count() << " ns elapsed\n";
    return( EXIT_SUCCESS );
}
