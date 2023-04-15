#ifndef _RAFTLIB_ORIG_HPP__
#define _RAFTLIB_ORIG_HPP__  1

#if RAFTLIB_ORIG

namespace raft
{
using Kernel = raft::kernel;
using DAG = raft::map;
using port_name_t = raft::port_key_name_t;
}
#define add_input input.addPort
#define add_output output.addPort
#define port_name_of_i( i ) raft::port_name_t( i, std::to_string( i ) )

#endif

#endif /* END _RAFTLIB_ORIG_HPP__ */
