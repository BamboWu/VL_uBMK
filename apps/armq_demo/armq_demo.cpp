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
#include <vector>
#include <algorithm>
#include <chrono>
#include <atomic>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#define NUM_ARRS 1000

struct Chunk
{
    Chunk() = default;

    Chunk( const Chunk &other )
    {
        (this)->buf = other.buf;
        (this)->seqnum = other.seqnum;
    }
    std::vector< uint8_t > buf;
    uint64_t seqnum;
};

class ChunkProcessingKernel : public raft::Kernel
{
public:
    ChunkProcessingKernel( const std::size_t chunk_size ) :
        raft::Kernel(), size( chunk_size )
    {
    }

    virtual ~ChunkProcessingKernel() = default;

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

protected:
    const std::size_t size;
};

class Generator : public ChunkProcessingKernel
{
public:
    Generator( const std::size_t chunk_size,
               const std::size_t delay_ticks,
               const std::size_t repetitions,
               int fanout ) :
        ChunkProcessingKernel( chunk_size ),
        delay( delay_ticks ),
        nreps( repetitions )
    {
#if RAFTLIB_ORIG
        for( auto i( 0 ); fanout > i; ++i )
        {
            add_output< Chunk >( port_name_of_i( i ) );
        }
#else
        add_output< Chunk >( "0"_port );
#endif
        rep = 0;
        uint8_t *flatted_arr = new uint8_t[ NUM_ARRS * size ];
        for( auto i( 0 ); NUM_ARRS > i; ++i )
        {
            arrs[ i ] = &flatted_arr[ i * size ];
            for( std::size_t j( 0 ); size > j; ++j )
            {
                arrs[ i ][ j ] = ( i + j ) % 52;
                if( 26 <= arrs[ i ][ j ] )
                {
                    arrs[ i ][ j ] += 'G'; /* 'a' - 26 */
                }
                else
                {
                    arrs[ i ][ j ] += 'A';
                }
            }
        }
        flatted_arr[ NUM_ARRS + size ] = 0; /* 1 / NUM_ARRS chance to spot 0 */
    }

    virtual ~Generator()
    {
        delete[] arrs[ 0 ];
    }

#if RAFTLIB_ORIG
    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        for( auto &port : (this)->output )
        {
            if( ! port.space_avail() )
            {
                continue;
            }
            auto beg_tick( rdtsc() );
            auto &chunk( port.template allocate< Chunk >() );
#else
            auto beg_tick( rdtsc() );
            auto &chunk( bufOut.allocate< Chunk >() );
#endif
            chunk.buf.resize( size );
            for( std::size_t idx( 0 ); size > idx; ++idx )
            {
                chunk.buf[ idx ] = arrs[ rep % NUM_ARRS ][ idx ];
            }
            chunk.seqnum = rep;
#if RAFTLIB_ORIG
            port.send();
#else
            bufOut.send();
#endif
            while( delay > ( rdtsc() - beg_tick ) )
            {
                /* busy doing nothing until delay reached */
            }
            if( nreps < ++rep )
            {
                return( raft::kstatus::stop );
            }
#if RAFTLIB_ORIG
        }
#endif
        return( raft::kstatus::proceed );
    }

private:
    const std::size_t delay;
    const std::size_t nreps;
    std::size_t rep;
    uint8_t *arrs[ NUM_ARRS ];
};

class Filter : public ChunkProcessingKernel
{
public:
    Filter( const std::size_t chunk_size ) :
        ChunkProcessingKernel( chunk_size )
    {
        add_input< Chunk >( "0"_port );
        add_output< Chunk >( "0"_port );
        verify_sum =
            size * size * ( size - 1 ) / 2 -
            ( size - 1 ) * size * ( 2 * size - 1 ) / 6;
    }

    Filter( const Filter &other ) :
        ChunkProcessingKernel( other.size ), verify_sum( other.verify_sum )
    {
        add_input< Chunk >( "0"_port );
        add_output< Chunk >( "0"_port );
    }

    virtual ~Filter() = default;

#if RAFTLIB_ORIG

    CLONE(); /* enable cloning */

    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        auto &chunk( input[ "0"_port ].template peek< Chunk >() );
#else
        auto &chunk( dataIn.template peek< Chunk >() );
#endif
        std::size_t count_sum = 0;
        bool seen_zero = false;
        for( std::size_t idx( 0 ); size > idx; ++idx )
        {
            if( 0 == chunk.buf[ idx ] )
            {
                seen_zero = true;
            }
            for( std::size_t countdown( idx ); 0 < countdown; --countdown )
            {
                count_sum += countdown;
            }
        }
        if( count_sum != verify_sum )
        {
            std::cout << count_sum << ", " << verify_sum << std::endl;
        }
#if RAFTLIB_ORIG
        if( seen_zero )
        {
            output[ "0"_port ].push( chunk );
        }
        input[ "0"_port ].recycle();
#else
        if( seen_zero )
        {
            bufOut.push( chunk );
        }
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }

private:

    std::size_t verify_sum;
};

class Copy : public ChunkProcessingKernel
{
public:
    Copy( const std::size_t chunk_size, int fanin ) :
        ChunkProcessingKernel( chunk_size )
    {
#if RAFTLIB_ORIG
        for( auto i( 0 ); fanin > i; ++i )
        {
            add_input< Chunk >( port_name_of_i( i ) );
        }
#else
        add_input< Chunk >( "0"_port );
#endif
        add_output< Chunk >( "0"_port );
    }

    virtual ~Copy() = default;

#if RAFTLIB_ORIG
    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        for( auto &port : (this)->input )
        {
            if( 0 < port.size() )
            {
                auto &chunk( port.template peek< Chunk >() );
                output[ "0"_port ].push( chunk );
                port.recycle();
            }
        }
#else
        auto &chunk( dataIn.template peek< Chunk >() );
        bufOut.push( chunk );
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }
};

class Count : public ChunkProcessingKernel
{
public:
    Count( const std::size_t chunk_size ) : ChunkProcessingKernel( chunk_size )
    {
        add_input< Chunk >( "0"_port );
    }

    virtual ~Count() = default;

#if RAFTLIB_ORIG
    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        auto &chunk( input[ "0"_port ].template peek< Chunk >() );
#else
        auto &chunk( dataIn.template peek< Chunk >() );
#endif
        std::size_t cnt = 0;
        for( auto &val : chunk.buf )
        {
            cnt += 'A' == val;
        }
        total_cnt.fetch_add( cnt, std::memory_order_relaxed );
#if RAFTLIB_ORIG
        input[ "0"_port ].recycle();
#else
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }

    std::atomic< std::size_t > total_cnt = { 0 };
};

int
main( int argc, char **argv )
{
    int repetitions = 1000;
    int F = 1; /* #filters */
    int C = 1; /* #copys */
    std::size_t D = 10000; /* delay_tick */
    std::size_t S = 60; /* chunk_size */
    if( argc < 2 )
    {
        std::cerr << "Usage: ./" << argv[0] <<
            " <orig|pw|cv|os|mix> [#repetitions=" << repetitions <<
            "] [F=" << F << "] [C=" << C <<
            "] [D=" << D << "] [S=" << S << "]\n";
        exit( EXIT_FAILURE );
    }
    else if( 2 < argc )
    {
        repetitions = atoi( argv[ 2 ] );
        if( 3 < argc )
        {
            F = atoi( argv[ 3 ] );
            if( 4 < argc )
            {
                C = atoi( argv[ 4 ] );
                if( 5 < argc )
                {
                    D = strtoull( argv[ 5 ], NULL, 0 );
                    if( 6 < argc )
                    {
                        S = strtoull( argv[ 6 ], NULL, 0 );
                    }
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
    if( 1 > F || 1 > C )
    {
        exit( EXIT_FAILURE );
    }

    Generator gen( S, D, repetitions, F );
    Filter filter( S );
    Copy copy( S, F );
    Count count( S );
    raft::DAG dag;

#if RAFTLIB_ORIG
    auto kernels( dag += gen <= filter >= copy );
    for( auto i( 1 ); C > i; ++i )
    {
        kernels = ( dag += kernels.getDst().first->get() >>
                    raft::kernel_maker< Copy >( S, 1 ) );
    }
    dag += kernels.getDst().first->get() >> count;
#else
    raft::Kpair *kpair( &( gen >> filter * F >> copy * 0 ) );
    for( auto i( 1 ); C > i; ++i )
    {
        kpair = &( *kpair >> *raft::kernel_maker< Copy >( S, 1 ) * 0 );
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
    if( "mix" == runtime_choice )
    {
        std::cout << "Mix\n";
        dag.exe< raft::RuntimeMix >();
    }
    else if ( "pw" == runtime_choice )
    {
        std::cout << "PW\n";
        dag.exe< raft::RuntimeFIFO >();
    }
    else if ( "os" == runtime_choice )
    {
        std::cout << "OneShot\n";
        dag.exe< raft::RuntimeNewBurst >();
    }
    else
    {
        std::cout << "CV\n";
        dag.exe< raft::RuntimeFIFOCV >();
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
