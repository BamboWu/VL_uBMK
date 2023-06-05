#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <atomic>

#include "threading.h"
#include "timing.h"
#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#ifdef VLINLINE
#include "vl/vl_inline.h"
#elif VL
#include "vl/vl.h"
#endif

#ifdef CAF
#include <assert.h>
#include "caf.h"
#endif

#ifdef ZMQ
#include <assert.h>
#include <zmq.h>
#endif

#ifdef BOOST
#include <vector>
#include <boost/lockfree/queue.hpp>
using boost_q_t = boost::lockfree::queue<double>;
std::vector<boost_q_t*> boost_queues;
#endif

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

void get_position(const int rank, const int pex, const int pey, int* myX,
                  int* myY) {
  *myX = rank % pex;
  *myY = rank / pex;
}

void get_neighbor(const int rank, const int pex, const int pey, const int x,
                  const int y, int *xUp, int *xDn, int *yUp, int *yDn) {
  *xUp = (x != (pex - 1)) ? rank + 1 : -1;
  *xDn = (x != 0) ? rank - 1 : -1;
  *yUp = (y != (pey - 1)) ? rank + pex : -1;
  *yDn = (y != 0) ? rank - pex : -1;
}

void get_queue_id(const int x, const int y, const int pex, const int pey,
                  int *xUpRx, int *xUpTx, int *xDnRx, int *xDnTx,
                  int *yUpRx, int *yUpTx, int *yDnRx, int *yDnTx) {
  int tmp;
  *xDnRx = x + y * (pex - 1);
  *xUpTx = *xDnRx + 1;
  tmp = pey * (pex - 1); /* number of horizontally up queues */
  *xUpRx = *xUpTx + tmp;
  *xDnTx = *xDnRx + tmp;
  tmp *= 2; /* number of horizontal queues */
  *yDnRx = y + x * (pey - 1) + tmp;
  *yUpTx = *yDnRx + 1;
  tmp = pex * (pey - 1); /* number of vertically up queues */
  *yUpRx = *yUpTx + tmp;
  *yDnTx = *yDnRx + tmp;
}

void compute(long sleep_nsec) {
  for (long i = 0; sleep_nsec > i; ++i) {
    __asm__ volatile("\
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
        :
        );
  }
}

/* Global variables */
int pex, pey, nthreads;
int repeats;
int msgSz;
long sleep_nsec;
long sleep_nsec2;
long burst_amp;
long burst_period;
long fast_period;
long slow_period;
int master_cacheline;
std::atomic< int > ready;
#ifdef ZMQ
void *ctx;
#endif

void sweep(const int xUp, const int xDn, const int yUp, const int yDn,
#ifdef ZMQ
           zmq_msg_t *msg,
           void *xUpSend, void *xDnSend, void *yUpSend, void *yDnSend,
           void *xUpRecv, void *xDnRecv, void *yUpRecv, void *yDnRecv
#elif BOOST
           double *msg,
           boost_q_t *xUpSend, boost_q_t *xDnSend, boost_q_t *yUpSend,
           boost_q_t *yDnSend, boost_q_t *xUpRecv, boost_q_t *xDnRecv,
           boost_q_t *yUpRecv, boost_q_t *yDnRecv
#elif VL
           double *msg,
           vlendpt_t *xUpSend, vlendpt_t *xDnSend, vlendpt_t *yUpSend,
           vlendpt_t *yDnSend, vlendpt_t *xUpRecv, vlendpt_t *xDnRecv,
           vlendpt_t *yUpRecv, vlendpt_t *yDnRecv
#elif CAF
           double *msg,
           cafendpt_t *xUpSend, cafendpt_t *xDnSend, cafendpt_t *yUpSend,
           cafendpt_t *yDnSend, cafendpt_t *xUpRecv, cafendpt_t *xDnRecv,
           cafendpt_t *yUpRecv, cafendpt_t *yDnRecv
#endif
           ) {

  int i;
#ifdef BOOST
  const int ndoubles = msgSz / sizeof(double);
  uint16_t idx;
#elif VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt, rcnt;
#elif CAF
  char *buf0 = new char[msgSz];
  char *buf1 = new char[msgSz];
  char *pmsg;
#endif
  for (i = 0; repeats > i; ++i) {

#if VL
    for (idx = 0; nblks > idx; ++idx) {
      /* vl has to break a message to fit into cacheline
       * and this loop cannot be inside, otherwise all producers goes first,
       * and it would overwhelm routing device producer buffer */
      *blkId = idx;
      cnt = (nblks - 1) > idx ? 56 : (msgSz - idx * 56);
#endif

    /* Sweep from (0,0) to (Px,Py) */
    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[5]));
      assert(msgSz == zmq_msg_recv(&msg[5], xDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(xDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf0, (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[7]));
      assert(msgSz == zmq_msg_recv(&msg[7], yDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(yDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf1, (void*)pmsg, msgSz);
#endif
    }

#ifdef VL
    if (0 == idx) { /* only compute with completed messages */
#endif
    compute(sleep_nsec);
#ifdef VL
    }
#endif

    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[0], msgSz));
      assert(msgSz == zmq_msg_send(&msg[0], xUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(xUpSend, (uint64_t)buf0);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf0, msgSz);
#endif
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[2], msgSz));
      assert(msgSz == zmq_msg_send(&msg[2], yUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(yUpSend, (uint64_t)buf1);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf1, msgSz);
#endif
#endif
    }

    /* Sweep from (Px,Py) to (0,0) */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[4]));
      assert(msgSz == zmq_msg_recv(&msg[4], xUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(xUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf0, (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[6]));
      assert(msgSz == zmq_msg_recv(&msg[6], yUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(yUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf1, (void*)pmsg, msgSz);
#endif
    }

#ifdef VL
    if (0 == idx) { /* only compute with completed messages */
#endif
    compute(sleep_nsec);
#ifdef VL
    }
#endif

    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[1], msgSz));
      assert(msgSz == zmq_msg_send(&msg[1], xDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(xDnSend, (uint64_t)buf0);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf0, msgSz);
#endif
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[3], msgSz));
      assert(msgSz == zmq_msg_send(&msg[3], yDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(yDnSend, (uint64_t)buf1);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf1, msgSz);
#endif
#endif
    }

    /* Sweep from (Px,0) to (0,Py) */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[4]));
      assert(msgSz == zmq_msg_recv(&msg[4], xUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(xUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf0, (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[7]));
      assert(msgSz == zmq_msg_recv(&msg[7], yDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(yDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf1, (void*)pmsg, msgSz);
#endif
    }

#ifdef VL
    if (0 == idx) { /* only compute with completed messages */
#endif
    compute(sleep_nsec);
#ifdef VL
    }
#endif

    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[1], msgSz));
      assert(msgSz == zmq_msg_send(&msg[1], xDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(xDnSend, (uint64_t)buf0);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf0, msgSz);
#endif
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[2], msgSz));
      assert(msgSz == zmq_msg_send(&msg[2], yUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(yUpSend, (uint64_t)buf1);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf1, msgSz);
#endif
#endif
    }

    /* Sweep from (0,Py) to (Px,0) */
    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[5]));
      assert(msgSz == zmq_msg_recv(&msg[5], xDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(xDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf0, (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[6]));
      assert(msgSz == zmq_msg_recv(&msg[6], yUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &rcnt);
#elif CAF
      caf_pop_strong(yUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf1, (void*)pmsg, msgSz);
#endif
    }

#ifdef VL
    if (0 == idx) { /* only compute with completed messages */
#endif
    compute(sleep_nsec);
#ifdef VL
    }
#endif

    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[0], msgSz));
      assert(msgSz == zmq_msg_send(&msg[0], xUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(xUpSend, (uint64_t)buf0);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf0, msgSz);
#endif
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[3], msgSz));
      assert(msgSz == zmq_msg_send(&msg[3], yDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      caf_push_strong(yDnSend, (uint64_t)buf1);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf1, msgSz);
#endif
#endif
    }

#ifdef VL
    }  /* end of for (idx = 0; nblks > idx; ++idx) */
#endif
  } /* end of for (i = 0; repeats > i; ++i) */

}

void halo(const int xUp, const int xDn, const int yUp, const int yDn,
#ifdef ZMQ
          zmq_msg_t *msg,
          void *xUpSend, void *xDnSend, void *yUpSend, void *yDnSend,
          void *xUpRecv, void *xDnRecv, void *yUpRecv, void *yDnRecv
#elif BOOST
          double *msg,
          boost_q_t *xUpSend, boost_q_t *xDnSend, boost_q_t *yUpSend,
          boost_q_t *yDnSend, boost_q_t *xUpRecv, boost_q_t *xDnRecv,
          boost_q_t *yUpRecv, boost_q_t *yDnRecv
#elif VL
          double *msg,
          vlendpt_t *xUpSend, vlendpt_t *xDnSend, vlendpt_t *yUpSend,
          vlendpt_t *yDnSend, vlendpt_t *xUpRecv, vlendpt_t *xDnRecv,
          vlendpt_t *yUpRecv, vlendpt_t *yDnRecv
#elif CAF
          double *msg,
          cafendpt_t *xUpSend, cafendpt_t *xDnSend, cafendpt_t *yUpSend,
          cafendpt_t *yDnSend, cafendpt_t *xUpRecv, cafendpt_t *xDnRecv,
          cafendpt_t *yUpRecv, cafendpt_t *yDnRecv
#endif
          ) {

  int i;
#ifdef BOOST
  const int ndoubles = msgSz / sizeof(double);
  uint16_t idx;
#elif VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt;
#elif CAF
  char *buf[8];
  for (int i = 0; 8 > i; ++i) {
    buf[i] = new char[msgSz];
  }
  char *pmsg;
#endif
  for (i = 0; repeats > i; ++i) {
    compute(sleep_nsec);

#ifdef VL
    for (idx = 0; nblks > idx; ++idx) {
      *blkId = idx;
      cnt = (nblks - 1) > idx ? 56 : (msgSz - idx * 56);
#endif

    /* send to four neighbours */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[0], msgSz));
      assert(msgSz == zmq_msg_send(&msg[0], xUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      memcpy((void*)buf[0], (void*)buf[4], msgSz);
      caf_push_strong(xUpSend, (uint64_t)buf[0]);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf[0], msgSz);
#endif
#endif
    }
    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[1], msgSz));
      assert(msgSz == zmq_msg_send(&msg[1], xDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      memcpy((void*)buf[1], (void*)buf[5], msgSz);
      caf_push_strong(xDnSend, (uint64_t)buf[1]);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf[1], msgSz);
#endif
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[2], msgSz));
      assert(msgSz == zmq_msg_send(&msg[2], yUpSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      memcpy((void*)buf[2], (void*)buf[6], msgSz);
      caf_push_strong(yUpSend, (uint64_t)buf[2]);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf[2], msgSz);
#endif
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(&msg[3], msgSz));
      assert(msgSz == zmq_msg_send(&msg[3], yDnSend, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnSend->push(*msg));
      }
#elif VL
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
#elif CAF
      memcpy((void*)buf[3], (void*)buf[7], msgSz);
      caf_push_strong(yDnSend, (uint64_t)buf[3]);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf[3], msgSz);
#endif
#endif
    }

    /* receive from four neighbors */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[4]));
      assert(msgSz == zmq_msg_recv(&msg[4], xUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
#elif CAF
      caf_pop_strong(xUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf[4], (void*)pmsg, msgSz);
#endif
    }
    if (-1 < xDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[5]));
      assert(msgSz == zmq_msg_recv(&msg[5], xDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!xDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
#elif CAF
      caf_pop_strong(xDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf[5], (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[6]));
      assert(msgSz == zmq_msg_recv(&msg[6], yUpRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yUpRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &cnt);
#elif CAF
      caf_pop_strong(yUpRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf[6], (void*)pmsg, msgSz);
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(0 == zmq_msg_init(&msg[7]));
      assert(msgSz == zmq_msg_recv(&msg[7], yDnRecv, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!yDnRecv->pop(*msg));
      }
#elif VL
      line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &cnt);
#elif CAF
      caf_pop_strong(yDnRecv, (uint64_t*)&pmsg);
      memcpy((void*)buf[7], (void*)pmsg, msgSz);
#endif
    }

#ifdef VL
    } /* endof for (idx = 0; nblks > idx; ++idx) */
#endif

  }
}

void incast(const bool isMaster,
#ifdef ZMQ
            zmq_msg_t *msg, void *queue
#elif BOOST
            double *msg, boost_q_t *queue
#elif VL
            double *msg, vlendpt_t *queue
#elif CAF
            double *msg, cafendpt_t *queue
#endif
            ) {
  int i, j;
#ifdef BOOST
  const int ndoubles = msgSz / sizeof(double);
  uint16_t idx;
#elif VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt;
#elif CAF
  char *buf = new char[msgSz];
  char *pmsg;
#endif
  for (i = 0; repeats > i; ++i) {

    if (isMaster) {
      for (j = nthreads - 1; 0 < j; --j) {
#ifdef ZMQ
        assert(msgSz == zmq_msg_recv(msg, queue, 0));
#elif BOOST
        for (idx = 0; ndoubles > idx; ++idx) {
          while (!queue->pop(*msg));
        }
#elif VL
        for (idx = 0; nblks > idx; ++idx) {
          line_vl_pop_weak(queue, (uint8_t*)buf, &cnt);
        }
#elif CAF
        caf_pop_strong(queue, (uint64_t*)&pmsg);
        memcpy((void*)buf, (void*)pmsg, msgSz);
#endif
        if (sleep_nsec) {
          compute(sleep_nsec);
        }
      }
    } else {
#ifdef ZMQ
      assert(0 == zmq_msg_init_size(msg, msgSz));
      assert(msgSz == zmq_msg_send(msg, queue, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!queue->push(*msg));
      }
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = (nblks - 1) > idx ? 56 : (msgSz - idx * 56);
        line_vl_push_strong(queue, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#elif CAF
      for (int i = 0; msgSz > i; ++i) {
        buf[i] = 0;
      }
      caf_push_strong(queue, (uint64_t)buf);
#ifdef CAF_PREPUSH
      caf_prepush((void*)buf, msgSz);
#endif
#endif
      long sleep_tmp =
          ((i % burst_period) > slow_period) ? -burst_amp : burst_amp;
      sleep_tmp += sleep_nsec2;
      if (0 < sleep_tmp) {
        compute(sleep_tmp);
      }
    }
  }
}

void outcast(const bool isMaster,
#ifdef ZMQ
            zmq_msg_t *msg, void *queue
#elif BOOST
            double *msg, boost_q_t *queue
#elif VL
            double *msg, vlendpt_t *queue
#elif CAF
            double *msg, cafendpt_t *queue
#endif
            ) {
  int i, j;
#ifdef BOOST
  const int ndoubles = msgSz / sizeof(double);
  uint16_t idx;
#elif VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt;
#elif CAF
  char *buf = new char[msgSz];
  char *pmsg;
#endif
  for (i = 0; repeats > i; ++i) {

    if (isMaster) {
      for (j = nthreads - 1; 0 < j; --j) {
#ifdef ZMQ
        assert(0 == zmq_msg_init_size(msg, msgSz));
        assert(msgSz == zmq_msg_send(msg, queue, 0));
#elif BOOST
        for (idx = 0; ndoubles > idx; ++idx) {
          while (!queue->push(*msg));
        }
#elif VL
        for (idx = 0; nblks > idx; ++idx) {
          *blkId = idx;
          cnt = (nblks - 1) > idx ? 56 : (msgSz - idx * 56);
          line_vl_push_strong(queue, (uint8_t*)buf, cnt + sizeof(uint16_t));
        }
#elif CAF
        for (int i = 0; msgSz > i; ++i) {
          buf[i] = 0;
        }
        caf_push_strong(queue, (uint64_t)buf);
#ifdef CAF_PREPUSH
        caf_prepush((void*)buf, msgSz);
#endif
#endif
        if (sleep_nsec) {
          compute(sleep_nsec);
        }
      }
    } else {
#ifdef ZMQ
      assert(msgSz == zmq_msg_recv(msg, queue, 0));
#elif BOOST
      for (idx = 0; ndoubles > idx; ++idx) {
        while (!queue->pop(*msg));
      }
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(queue, (uint8_t*)buf, &cnt);
      }
#elif CAF
      caf_pop_strong(queue, (uint64_t*)&pmsg);
      memcpy((void*)buf, (void*)pmsg, msgSz);
#endif
      long sleep_tmp =
          ((i % burst_period) > slow_period) ? -burst_amp : burst_amp;
      sleep_tmp += sleep_nsec2;
      if (0 < sleep_tmp) {
        compute(sleep_tmp);
      }
    }
  }
}


void *worker(void *arg) {
  int *pid = (int*) arg;
  //pinAtCoreFromList(*pid);

  bool isMaster = (0 == *pid);
#if EMBER_INCAST || EMBER_OUTCAST
  if (isMaster) {
    printf("slow/fast span: %4ld / %4ld\n", slow_period, fast_period);
    printf("slave sleep:   %4ld +- %4ld\n", sleep_nsec2, burst_amp);
    printf("master sleep:          %4ld\n", sleep_nsec);
    printf("master cachelines:      %3d\n", master_cacheline);
  }
#ifdef ZMQ
  void *queue;
  zmq_msg_t msg;
  if (isMaster) {
#ifdef EMBER_INCAST
    queue = zmq_socket(ctx, ZMQ_PULL);
    assert(0 == zmq_msg_init(&msg));
#else
    queue = zmq_socket(ctx, ZMQ_PUSH);
#endif
    assert(0 == zmq_bind(queue, "inproc://0"));
  } else {
#ifdef EMBER_INCAST
    queue = zmq_socket(ctx, ZMQ_PUSH);
#else
    queue = zmq_socket(ctx, ZMQ_PULL);
    assert(0 == zmq_msg_init(&msg));
#endif
    assert(0 == zmq_connect(queue, "inproc://0"));
  }
#elif BOOST
  double msg;
  boost_q_t *queue = boost_queues[0];
#elif VL
  double msg;
  vlendpt_t endpt;
  vlendpt_t *queue = &endpt;
  if (isMaster) {
#ifdef EMBER_INCAST
    open_byte_vl_as_consumer(1, queue, master_cacheline);
#else
    open_byte_vl_as_producer(1, queue, 1);
#endif
  } else {
#ifdef EMBER_INCAST
    open_byte_vl_as_producer(1, queue, 1);
#else
    open_byte_vl_as_consumer(1, queue, 1);
#endif
  }
#elif CAF
  double msg;
  cafendpt_t endpt;
  cafendpt_t *queue = &endpt;
  open_caf(1, queue);
#endif
#else /* NOT EMBER_INCAST */
  int x, y, xUp, xDn, yUp, yDn;
  int xUpRx, xUpTx, xDnRx, xDnTx, yUpRx, yUpTx, yDnRx, yDnTx;

  get_position(*pid, pex, pey, &x, &y);
  get_neighbor(*pid, pex, pey, x, y, &xUp, &xDn, &yUp, &yDn);
  get_queue_id(x, y, pex, pey, &xUpRx, &xUpTx, &xDnRx, &xDnTx,
               &yUpRx, &yUpTx, &yDnRx, &yDnTx);
#ifdef ZMQ
  void *xUpSend = zmq_socket(ctx, ZMQ_PUSH);
  void *xDnSend = zmq_socket(ctx, ZMQ_PUSH);
  void *yUpSend = zmq_socket(ctx, ZMQ_PUSH);
  void *yDnSend = zmq_socket(ctx, ZMQ_PUSH);
  void *xUpRecv = zmq_socket(ctx, ZMQ_PULL);
  void *xDnRecv = zmq_socket(ctx, ZMQ_PULL);
  void *yUpRecv = zmq_socket(ctx, ZMQ_PULL);
  void *yDnRecv = zmq_socket(ctx, ZMQ_PULL);
  char queue_str[64];
  zmq_msg_t msg[8];
  if (-1 < xUp) {
    sprintf(queue_str, "inproc://%d", xUpTx);
    assert(0 == zmq_bind(xUpSend, queue_str));
    sprintf(queue_str, "inproc://%d", xUpRx);
    assert(0 == zmq_connect(xUpRecv, queue_str));
  }
  if (-1 < xDn) {
    sprintf(queue_str, "inproc://%d", xDnTx);
    assert(0 == zmq_bind(xDnSend, queue_str));
    sprintf(queue_str, "inproc://%d", xDnRx);
    assert(0 == zmq_connect(xDnRecv, queue_str));
  }
  if (-1 < yUp) {
    sprintf(queue_str, "inproc://%d", yUpTx);
    assert(0 == zmq_bind(yUpSend, queue_str));
    sprintf(queue_str, "inproc://%d", yUpRx);
    assert(0 == zmq_connect(yUpRecv, queue_str));
  }
  if (-1 < yDn) {
    sprintf(queue_str, "inproc://%d", yDnTx);
    assert(0 == zmq_bind(yDnSend, queue_str));
    sprintf(queue_str, "inproc://%d", yDnRx);
    assert(0 == zmq_connect(yDnRecv, queue_str));
  }
#elif BOOST
  char msgBuf[msgSz];
  double* msg = (double*) msgBuf;
  boost_q_t *xUpSend = nullptr;
  boost_q_t *xUpRecv = nullptr;
  boost_q_t *xDnSend = nullptr;
  boost_q_t *xDnRecv = nullptr;
  boost_q_t *yUpSend = nullptr;
  boost_q_t *yUpRecv = nullptr;
  boost_q_t *yDnSend = nullptr;
  boost_q_t *yDnRecv = nullptr;

  if (-1 < xUp) {
    xUpSend = boost_queues[xUpTx];
    xUpRecv = boost_queues[xUpRx];
  }
  if (-1 < xDn) {
    xDnSend = boost_queues[xDnTx];
    xDnRecv = boost_queues[xDnRx];
  }
  if (-1 < yUp) {
    yUpSend = boost_queues[yUpTx];
    yUpRecv = boost_queues[yUpRx];
  }
  if (-1 < yDn) {
    yDnSend = boost_queues[yDnTx];
    yDnRecv = boost_queues[yDnRx];
  }
#elif VL
  char msgBuf[msgSz];
  double* msg = (double*) msgBuf;
  vlendpt_t endpts[8];
  vlendpt_t *xUpSend = &endpts[0];
  vlendpt_t *xDnSend = &endpts[1];
  vlendpt_t *yUpSend = &endpts[2];
  vlendpt_t *yDnSend = &endpts[3];
  vlendpt_t *xUpRecv = &endpts[4];
  vlendpt_t *xDnRecv = &endpts[5];
  vlendpt_t *yUpRecv = &endpts[6];
  vlendpt_t *yDnRecv = &endpts[7];
  if (-1 < xUp) {
    open_byte_vl_as_producer(xUpTx, xUpSend, 1);
    open_byte_vl_as_consumer(xUpRx, xUpRecv, 1);
  }
  if (-1 < xDn) {
    open_byte_vl_as_producer(xDnTx, xDnSend, 1);
    open_byte_vl_as_consumer(xDnRx, xDnRecv, 1);
  }
  if (-1 < yUp) {
    open_byte_vl_as_producer(yUpTx, yUpSend, 1);
    open_byte_vl_as_consumer(yUpRx, yUpRecv, 1);
  }
  if (-1 < yDn) {
    open_byte_vl_as_producer(yDnTx, yDnSend, 1);
    open_byte_vl_as_consumer(yDnRx, yDnRecv, 1);
  }
#elif CAF
  char msgBuf[msgSz];
  double* msg = (double*) msgBuf;
  cafendpt_t endpts[8];
  cafendpt_t *xUpSend = &endpts[0];
  cafendpt_t *xDnSend = &endpts[1];
  cafendpt_t *yUpSend = &endpts[2];
  cafendpt_t *yDnSend = &endpts[3];
  cafendpt_t *xUpRecv = &endpts[4];
  cafendpt_t *xDnRecv = &endpts[5];
  cafendpt_t *yUpRecv = &endpts[6];
  cafendpt_t *yDnRecv = &endpts[7];
  if (-1 < xUp) {
    open_caf(xUpTx, xUpSend);
    open_caf(xUpRx, xUpRecv);
  }
  if (-1 < xDn) {
    open_caf(xDnTx, xDnSend);
    open_caf(xDnRx, xDnRecv);
  }
  if (-1 < yUp) {
    open_caf(yUpTx, yUpSend);
    open_caf(yUpRx, yUpRecv);
  }
  if (-1 < yDn) {
    open_caf(yDnTx, yDnSend);
    open_caf(yDnRx, yDnRecv);
  }
#endif

#endif /* EMBER_INCAST */

  if (isMaster) {
    while( (nthreads - 1) != ready ){
      compute(100);
    }

#ifndef NOGEM5
    m5_reset_stats(0, 0);
#endif

    ready++;
  } else {
    ready++;
    while( nthreads != ready ){
      compute(*pid * 10);
    };
  }

#ifdef EMBER_INCAST
  incast(isMaster, &msg, queue);
#elif EMBER_OUTCAST
  outcast(isMaster, &msg, queue);
#elif EMBER_SWEEP2D
  sweep(xUp, xDn, yUp, yDn, msg,
       xUpSend, xDnSend, yUpSend, yDnSend, xUpRecv, xDnRecv, yUpRecv, yDnRecv);
#elif EMBER_HALO2D
  halo(xUp, xDn, yUp, yDn, msg,
       xUpSend, xDnSend, yUpSend, yDnSend, xUpRecv, xDnRecv, yUpRecv, yDnRecv);
#endif

  /* comment out on purpose to exclude this from ROI.
#ifdef EMBER_INCAST
#ifdef ZMQ
  assert(0 == zmq_close(queue));
#elif VL
  if (isMaster) {
    close_byte_vl_as_consumer(queue);
  } else {
    close_byte_vl_as_producer(queue);
  }
#endif
#else
#ifdef ZMQ
  assert(0 == zmq_close(xUpSend));
  assert(0 == zmq_close(xUpRecv));
  assert(0 == zmq_close(xDnSend));
  assert(0 == zmq_close(xDnRecv));
  assert(0 == zmq_close(yUpSend));
  assert(0 == zmq_close(yUpRecv));
  assert(0 == zmq_close(yDnSend));
  assert(0 == zmq_close(yDnRecv));
#elif VL
  if (-1 < xUp) {
    close_byte_vl_as_producer(xUpSend);
    close_byte_vl_as_consumer(xUpRecv);
  }
  if (-1 < xDn) {
    close_byte_vl_as_producer(xDnSend);
    close_byte_vl_as_consumer(xDnRecv);
  }
  if (-1 < yUp) {
    close_byte_vl_as_producer(yUpSend);
    close_byte_vl_as_consumer(yUpRecv);
  }
  if (-1 < yDn) {
    close_byte_vl_as_producer(yDnSend);
    close_byte_vl_as_consumer(yDnRecv);
  }
#endif

  free(xRecvBuffer);
  free(xSendBuffer);
  free(yRecvBuffer);
  free(ySendBuffer);
  */

  return NULL;
}

int main(int argc, char* argv[]) {
  int i;
  pex = pey = 2; /* default values */
  repeats = 7;
  msgSz = 7 * sizeof(double);
  sleep_nsec = 1000;
  sleep_nsec2 = 1000;
  burst_amp = 0;
  fast_period = 1;
  slow_period = 1;
  master_cacheline = 32;
  parseCoreList("0-3");
  for (i = 0; argc > i; ++i) {
    if (0 == strcmp("-pex", argv[i])) {
      pex = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-pey", argv[i])) {
      pey = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-iterations", argv[i])) {
      repeats = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-sleep2", argv[i])) {
      sleep_nsec2 = atol(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-sleep", argv[i])) {
      sleep_nsec = atol(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-msgSz", argv[i])) {
      msgSz = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-burst_amp", argv[i])) {
      burst_amp = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-fast_period", argv[i])) {
      fast_period = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-slow_period", argv[i])) {
      slow_period = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-master_cacheline", argv[i])) {
      master_cacheline = atoi(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-core_list", argv[i])) {
      parseCoreList(argv[i + 1]);
      ++i;
    }
  }
  pinAtCoreFromList(0);
  printf("Px x Py:        %4d x %4d\n", pex, pey);
  printf("Message Size:         %5d\n", msgSz);
  printf("Iterations:           %5d\n", repeats);
  nthreads = pex * pey;
  burst_period = slow_period + fast_period;
  ready = 0;

#ifdef EMBER_INCAST

#ifdef ZMQ
  ctx = zmq_ctx_new();
  assert(ctx);
#elif BOOST
  boost_queues.resize(1);
  boost_queues[0] = new boost_q_t(msgSz / sizeof(double));
#elif VL
  mkvl();
#endif

#else /* NOT EMBER_INCAST */

#ifdef ZMQ
  ctx = zmq_ctx_new();
  assert(ctx);
#elif BOOST
  boost_queues.resize(pex * pey * 4 - (pex + pey) * 2 + 1);
  for (i = 0; (pex * pey * 4) - (pex + pey) * 2 >= i; ++i) {
    boost_queues[i] = new boost_q_t(msgSz / sizeof(double));
  }
#elif VL
  for (i = 0; (pex * pey * 4) - (pex + pey) * 2 > i; ++i) {
    mkvl();
  }
#endif

#endif /* EMBER_INCAST */

  pthread_t threads[nthreads];
  int ids[nthreads];
  for (i = 1; nthreads > i; ++i) {
    ids[i] = i;
    threadCreate(&threads[i], NULL, worker, (void *)&ids[i], i);
  }

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());
  ids[0] = 0;
  worker((void*)&ids[0]); // main thread itself as the master worker

  for (i = 1; nthreads > i; ++i) {
    pthread_join(threads[i], NULL);
  }

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast< nanoseconds >(end - beg));

  printf("%lu ticks elapsed\n%lu ns elapsed\n",
         (end_tsc - beg_tsc), elapsed.count());

#ifdef ZMQ
  /* close sockets are commented out on purpose, comment out this to exit.
  assert(0 == zmq_ctx_term(ctx));
  */
#endif
  return 0;
}
