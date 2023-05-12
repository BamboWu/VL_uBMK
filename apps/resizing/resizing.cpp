#include <cstdint>
#include <chrono>
#include <raft>

#include "timing.h"

#if RAFTLIB_ORIG
#include "raftlib_orig.hpp"
#endif

#ifndef L1D_CACHE_LINE_SIZE
#define L1D_CACHE_LINE_SIZE 64
#endif

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#define NRINGBUFFERS 1000000
#define MAX_EXP 4

template<int SIZE>
struct Msg {
    uint8_t buf[SIZE];
    Msg() = default;
    Msg(const Msg &other) = default;
};

typedef Msg<128> LargeMsg;
typedef Msg<64> BlkMsg;
typedef uint8_t ByteMsg;

#if RAFTLIB_ORIG
template<class T>
using RingBufferT = RingBuffer<T>;
#define INIT_SIZE 64
#else
template<class T>
using RingBufferT = raft::RingBufferLinked<T>;
#define INIT_SIZE 32
#endif

int main() {

    nanoseconds ns[3][MAX_EXP]; /* [Byte/Blk/Large][64->128,->256,...] */

    volatile bool exit_alloc = false;
    raft::FIFO *large_rbs[NRINGBUFFERS];
    raft::FIFO *blk_rbs[NRINGBUFFERS];
    raft::FIFO *byte_rbs[NRINGBUFFERS];
#if RAFTLIB_ORIG
    LargeMsg large_msg;
    BlkMsg blk_msg;
    ByteMsg byte_msg;
    ptr_map_t gc_map;
    ptr_set_t gc_set;
#endif
    for (std::size_t i = 0; NRINGBUFFERS > i; ++i) {
        large_rbs[i] = RingBufferT<LargeMsg>::make_new_fifo(
                INIT_SIZE, L1D_CACHE_LINE_SIZE, nullptr);
#if RAFTLIB_ORIG
        large_rbs[i]->setPtrMap(&gc_map);
        large_rbs[i]->setPtrSet(&gc_set);
        large_rbs[i]->setInPeekSet(&gc_set);
        large_rbs[i]->setOutPeekSet(&gc_set);
        large_rbs[i]->push(large_msg); /* RingBuffer resize condition */
#endif
        blk_rbs[i] = RingBufferT<BlkMsg>::make_new_fifo(
                INIT_SIZE, L1D_CACHE_LINE_SIZE, nullptr);
#if RAFTLIB_ORIG
        blk_rbs[i]->setPtrMap(&gc_map);
        blk_rbs[i]->setPtrSet(&gc_set);
        blk_rbs[i]->setInPeekSet(&gc_set);
        blk_rbs[i]->setOutPeekSet(&gc_set);
        blk_rbs[i]->push(blk_msg); /* RingBuffer resize condition */
#endif
        byte_rbs[i] = RingBufferT<ByteMsg>::make_new_fifo(
                INIT_SIZE, L1D_CACHE_LINE_SIZE, nullptr);
#if RAFTLIB_ORIG
        byte_rbs[i]->setPtrMap(&gc_map);
        byte_rbs[i]->setPtrSet(&gc_set);
        byte_rbs[i]->setInPeekSet(&gc_set);
        byte_rbs[i]->setOutPeekSet(&gc_set);
        byte_rbs[i]->push(byte_msg); /* RingBuffer resize condition */
#endif
    }

    for (int exp = 0; MAX_EXP > exp; ++exp) {
        std::size_t size_to_be = 1 << (exp + 7);
        std::cout << size_to_be << std::endl;
        const auto beg0(high_resolution_clock::now());
        for (std::size_t i = 0; NRINGBUFFERS > i; ++i) {
            large_rbs[i]->resize(size_to_be, L1D_CACHE_LINE_SIZE, exit_alloc);
        }
        const auto end0(high_resolution_clock::now());
        ns[2][exp] = duration_cast<nanoseconds>(end0 - beg0);

        const auto beg1(high_resolution_clock::now());
        for (std::size_t i = 0; NRINGBUFFERS > i; ++i) {
            blk_rbs[i]->resize(size_to_be, L1D_CACHE_LINE_SIZE, exit_alloc);
        }
        const auto end1(high_resolution_clock::now());
        ns[1][exp] = duration_cast<nanoseconds>(end1 - beg1);

        const auto beg2(high_resolution_clock::now());
        for (std::size_t i = 0; NRINGBUFFERS > i; ++i) {
            byte_rbs[i]->resize(size_to_be, L1D_CACHE_LINE_SIZE, exit_alloc);
        }
        const auto end2(high_resolution_clock::now());
        ns[0][exp] = duration_cast<nanoseconds>(end2 - beg2);
    }

    for (int exp = 0; MAX_EXP > exp; ++exp) {
        std::cout << ns[0][exp].count() << " " << ns[1][exp].count() << " " <<
            ns[2][exp].count() << std::endl;
    }

    return 0;
}
