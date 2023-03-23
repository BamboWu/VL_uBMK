#ifndef _RAFTLIB_ORIG_HPP__
#define _RAFTLIB_ORIG_HPP__  1

#if RAFTLIB_ORIG

namespace raft
{
using Kernel = raft::kernel;
using DAG = raft::map;
}
#define add_input input.addPort
#define add_output output.addPort

#endif

#endif /* END _RAFTLIB_ORIG_HPP__ */
