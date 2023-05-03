/**
 * armq_demo.hpp -
 * @author: Qinzhe Wu
 * @version: Mon Apr 24 17:40:03 2023
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

#ifndef ARMQ_DEMO_HPP__
#define ARMQ_DEMO_HPP__
#include <raft>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#define NUM_ARRS 1000000
#ifndef L1D_CACHE_LINE_SIZE
#define L1D_CACHE_LINE_SIZE 64
#endif

#define NVALS_PER_LINE ( L1D_CACHE_LINE_SIZE / sizeof( std::size_t ) )

struct Chunk
{
    Chunk() = default;

    Chunk( const Chunk &other ) = default;
    std::size_t *buf;
    std::size_t seqnum;
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

    inline void touch( Chunk &chunk, std::size_t nrounds, std::size_t nnops )
    {
        std::size_t nop_rounds( nnops / nrounds / 10 );
        for( std::size_t i( 0 ); nrounds > i; ++i )
        {
            std::size_t pos = 0;
            // a chasing pointer pattern to create some cache pressure
            do
            {
                chunk.buf[ pos + 1 ] = chunk.buf[ pos ];
                pos = chunk.buf[ pos ];
            } while( 0 != pos );
            // add a bit more work by nops
            for( std::size_t j( 0 ); nop_rounds > j; ++j )
            {
                __asm__ volatile( "\
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
                        : );
            }
        }
    }

    const std::size_t size;
};

class Sequencer : public raft::Kernel
{
public:
    Sequencer( const std::size_t repetitions, int fanout ) :
        raft::Kernel(), nreps( repetitions ), nfanouts( fanout )
    {
#if RAFTLIB_ORIG
        for( auto i( 0 ); fanout > i; ++i )
        {
            add_output< std::size_t >( port_name_of_i( i ) );
        }
#else
        add_output< std::size_t >( "0"_port );
#endif
        rep = 0;
    }

    virtual ~Sequencer() = default;

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
#if RAFTLIB_ORIG
#if STDALLOC
            if( ! port.space_avail() )
            {
                continue;
            }
#else /* let dynalloc trigger resize */
#endif
            port.push( rep );
#else /* RAFTLIB_ORIG */
            bufOut.push( rep );
#endif
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

private:
    const std::size_t nreps;
    std::size_t rep;
    int nfanouts;
};

class Generator : public ChunkProcessingKernel
{
public:
    Generator( const std::size_t chunk_size,
               const std::size_t delay_ticks ) :
        ChunkProcessingKernel( chunk_size ),
        delay( delay_ticks )
    {
        add_input< std::size_t >( "0"_port );
        add_output< Chunk >( "0"_port );
    }

    Generator( const Generator &other ) :
        ChunkProcessingKernel( other.size ),
        delay( other.delay )
    {
        add_input< std::size_t >( "0"_port );
        add_output< Chunk >( "0"_port );
    }

    virtual ~Generator() = default;

#if RAFTLIB_ORIG

    CLONE(); /* enable cloning */

    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
        std::size_t seqnum;
#if RAFTLIB_ORIG
        input[ "0"_port ].pop( seqnum );
#else
        dataIn.pop( seqnum );
#endif
        auto beg_tick( rdtsc() );
#if RAFTLIB_ORIG
        auto &chunk( output[ "0"_port ].template allocate< Chunk >() );
#else
        auto &chunk( bufOut.allocate< Chunk >() );
#endif
        chunk.buf = arrs[ seqnum % NUM_ARRS ];
        chunk.seqnum = seqnum;
        for( std::size_t i( 0 ); size > i; ++i )
        {
            chunk.buf[ i * NVALS_PER_LINE + 1 ] =
                chunk.buf[ i * NVALS_PER_LINE ];
        }
        while( delay > ( rdtsc() - beg_tick ) )
        {
            /* busy doing nothing until delay reached */
        }
#if RAFTLIB_ORIG
        output[ "0"_port ].send();
#else
        bufOut.send();
#endif
        return( raft::kstatus::proceed );
    }

    static void populate( const std::size_t size )
    {
        srand( size );
        std::size_t *flatted_arr =
            new std::size_t[ NUM_ARRS * size * NVALS_PER_LINE ];
        std::size_t pos;
        // create a pool of offsets
        std::vector< std::size_t > offsets( size );
        for( std::size_t i( 0 ); size > i; ++i )
        {
            offsets[ i ] = i;
        }
        for( std::size_t i( 0 ); NUM_ARRS > i; ++i )
        {
            // make arrs[i] the shortcuts to the i-th segement
            arrs[ i ] = &flatted_arr[ i * size * NVALS_PER_LINE ];

            // populate the arrs[ i ] to form a chain
            pos = 0; // begin with position 0
            for( std::size_t j( 0 ); ( size - 1 ) > j; ++j )
            {
                std::size_t rand_idx;
                do
                {
                    rand_idx = rand() % ( size - j );
                } while( 0 == rand_idx || // do not point to 0
                         offsets[ rand_idx ] == pos ); // do not point to self
                arrs[ i ][ pos * NVALS_PER_LINE ] =
                    offsets[ rand_idx ] * NVALS_PER_LINE;
                pos = offsets[ rand_idx ];
                // swap the used offset to tail, so it would not be used again
                std::swap( offsets[ rand_idx ], offsets[ size - j - 1 ] );
            }
            arrs[ i ][ pos * NVALS_PER_LINE ] = 0;
            // last slot unset loops back to position 0
        }
    }

private:

    const std::size_t delay;
    static std::size_t *arrs[ NUM_ARRS ];
};

class Filter : public ChunkProcessingKernel
{
public:
    Filter( const std::size_t chunk_size,
            const std::size_t rounds,
            const std::size_t nops,
            int fanin ) :
        ChunkProcessingKernel( chunk_size ),
        nrounds( rounds ),
        nnops( nops )
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

    Filter( const Filter &other ) :
        ChunkProcessingKernel( other.size ),
        nrounds( other.nrounds ),
        nnops( other.nnops )
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
        for( auto &port : (this)->input )
        {
            if( 0 < port.size() )
            {
                auto &chunk( port.template peek< Chunk >() );
                touch( chunk, nrounds, nnops );
                if( ( ( chunk.seqnum / 1000 ) % 1000 ) ==
                    ( chunk.seqnum % 1000 ) )
                {
                    output[ "0"_port ].push( chunk );
                }
                port.recycle();
            }
        }
#else
        auto &chunk( dataIn.template peek< Chunk >() );
        touch( chunk, nrounds, nnops );
        if( ( ( chunk.seqnum / 1000 ) % 1000 ) == ( chunk.seqnum % 1000 ) )
        {
            bufOut.push( chunk );
        }
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }
private:
    const std::size_t nrounds;
    const std::size_t nnops;
};

class Copy : public ChunkProcessingKernel
{
public:
    Copy( const std::size_t chunk_size,
          int fanin ) :
        ChunkProcessingKernel( chunk_size ),
        nfanins( fanin )
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
    Copy( const Copy &other ) :
        ChunkProcessingKernel( other.size ),
        nfanins( other.nfanins )
    {
#if RAFTLIB_ORIG
        for( auto i( 0 ); nfanins > i; ++i )
        {
            add_input< Chunk >( port_name_of_i( i ) );
        }
#else
        add_input< Chunk >( "0"_port );
#endif
        add_output< Chunk >( "0"_port );
    }

    CLONE();

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
                touch( chunk, 1, 0 );
                output[ "0"_port ].push( chunk );
                port.recycle();
            }
        }
#else
        auto &chunk( dataIn.template peek< Chunk >() );
        touch( chunk, 1, 0 );
        bufOut.push( chunk );
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }

    const int nfanins;
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
        std::size_t pos = 0;
        do
        {
            cnt += 0 == pos;
            pos = chunk.buf[ pos ];
        } while( 0 != pos );
        total_cnt.fetch_add( cnt, std::memory_order_relaxed );
        //delete[] chunk.buf;
#if RAFTLIB_ORIG
        input[ "0"_port ].recycle();
#else
        dataIn.recycle();
#endif
        return( raft::kstatus::proceed );
    }

    std::atomic< std::size_t > total_cnt = { 0 };
};

#endif /* end of ARMQ_DEMO_HPP__ */
