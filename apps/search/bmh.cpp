/**
 * bmh.cpp -
 * @author: Jonathan Beard
 * @version: Sat Oct 17 10:36:03 2015
 *
 * Copyright 2015 Jonathan Beard
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
#include <raftio>
#include <cstdlib>
#include <cstdint>
#include <string>
#include <iterator>
#include <algorithm>
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

template < class T > class search : public raft::Kernel
{
public:
    search( const std::string && term ) : raft::Kernel(),
                                          term_length( term.length() ),
                                          term( term )
    {
        add_input< T >( "0"_port );
        add_output< std::size_t >( "0"_port );
    }

    search( const std::string &term ) : raft::Kernel(),
                                        term_length( term.length() ),
                                        term( term )
    {
        add_input< T >( "0"_port );
        add_output< std::size_t >( "0"_port );
    }

    virtual ~search() = default;

#if RAFTLIB_ORIG
    virtual raft::kstatus run()
#else
    virtual raft::kstatus::value_t compute( raft::StreamingData &dataIn,
                                            raft::StreamingData &bufOut )
#endif
    {
#if RAFTLIB_ORIG
        auto &chunk( input[ "0"_port ].template peek< T >() );
#else
        auto &chunk( dataIn[ "0"_port ].template peek< T >() );
#endif
        auto it( chunk.begin() );
        do
        {
            it = std::search( it, chunk.end(),
                              term.begin(), term.end() );
            if( it != chunk.end() )
            {
#if RAFTLIB_ORIG
                output[ "0"_port ].push( it.location() );
#else
                bufOut[ "0"_port ].push( it.location() );
#endif
                it += 1;
            }
            else
            {
                break;
            }
        }
        while( true );
#if RAFTLIB_ORIG
        input[ "0"_port ].recycle();
#else
        dataIn[ "0"_port ].recycle();
#endif
        return( raft::kstatus::proceed );
    }

#if RAFTLIB_ORIG
#else
    virtual bool pop( raft::Task *task, bool dryrun )
    {
        return task->pop( "0"_port, dryrun );
    }

    virtual bool allocate( raft::Task *task, bool dryrun )
    {
        return task->allocate( "0"_port, dryrun );
    }
#endif

private:
    const std::size_t term_length;
    const std::string term;
};

int
main( int argc, char **argv )
{
    using chunk = raft::filechunk< 38 /** 38B data, 24B meta == 62B payload **/ >;
    std::cerr << "chunk size: " << sizeof( chunk ) << "\n";
    using fr = raft::filereader< chunk, false >;
    using search = search< chunk >;
    using print = raft::print< std::size_t, '\n'>;

    const std::string term( argv[ 2 ] );
    int kernel_count = 1;
    int repetitions = 1;
    raft::DAG dag;
    if( argc < 3 )
    {
        std::cerr << "Usage: ./search <file.txt> <token> [#threads=1]\n";
        exit( EXIT_FAILURE );
    } else if ( 4 <= argc )
    {
        kernel_count = atoi( argv[ 3 ] );
        if ( 5 <= argc )
        {
            repetitions = atoi( argv[ 4 ] );
        }
    }

    fr read( argv[ 1 ], (fr::offset_type) term.length(),
#if RAFTLIB_ORIG
                         kernel_count,
#endif
                         repetitions );
#if RAFTLIB_ORIG
    print p( kernel_count );
    for( auto i( 0 ); i < kernel_count; ++i )
    {
#if STRING_NAMES
        dag += read[ std::to_string( i ) ] >>
            raft::order::in >> raft::kernel_maker< search >( term ) >>
            p[ std::to_string( i ) ];
#else
        dag += read[ raft::port_key_name_t( i, std::to_string( i ) ) ] >>
            raft::order::in >> raft::kernel_maker< search >( term ) >>
            p[ raft::port_key_name_t( i, std::to_string( i ) ) ];
#endif
    }
#else
    search s( term );
    print p;
    dag += read >> s * kernel_count >> p;
#endif

    const uint64_t beg_tsc = rdtsc();
    const auto beg( high_resolution_clock::now() );

    dag.exe<
#if RAFTLIB_ORIG
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
             no_parallel
#else // not( RAFTLIB_ORIG )
#if ONESHOT
             raft::RuntimeFIFOOneShot
#else
             raft::RuntimeFIFO
#endif
#endif // RAFTLIB_ORIG
           >();

    const uint64_t end_tsc = rdtsc();
    const auto end( high_resolution_clock::now() );
    const auto elapsed( duration_cast< nanoseconds >( end - beg ) );
    std::cout << ( end_tsc - beg_tsc ) << " ticks elapsed\n";
    std::cout << elapsed.count() << " ns elapsed\n";
    return( EXIT_SUCCESS );
}
