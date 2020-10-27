#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include <atomic>

#include "threading.h"
#include "timing.h"
#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#ifdef VL
#include "vl/vl.h"
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
  struct timespec sleepTS;
  struct timespec remainTS;
  sleepTS.tv_sec = 0;
  sleepTS.tv_nsec = sleep_nsec;
  if (EINTR == nanosleep(&sleepTS, &remainTS)) {
    while (EINTR == nanosleep(&remainTS, &remainTS));
  }
}

/* Global variables */
int pex, pey, nthreads;
int repeats;
int msgSz;
long sleep_nsec;
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
#endif
  for (i = 0; repeats > i; ++i) {

#if VL
    for (idx = 0; nblks > idx; ++idx) { /* } */
      /* vl has to break a message to fit into cacheline
       * and this loop cannot be inside, otherwise all producers goes first,
       * and it would overwhelm routing device producer buffer */
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
      line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
#endif
  for (i = 0; repeats > i; ++i) {
    compute(sleep_nsec);

#ifdef VL
    for (idx = 0; nblks > idx; ++idx) {
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
      *blkId = idx;
      cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
      line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
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
#endif
  for (i = 0; repeats > i; ++i) {
    compute(sleep_nsec);

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
#endif
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
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        line_vl_push_strong(queue, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
  }
}

void *worker(void *arg) {
  int *pid = (int*) arg;
  setAffinity(*pid);

#ifdef EMBER_INCAST
  bool isMaster = (0 == *pid);
  double *buffer;
  if (isMaster) {
    buffer = (double*)malloc(nthreads * msgSz);
  } else {
    buffer = (double*)malloc(msgSz);
  }
#ifdef ZMQ
  void *queue;
  zmq_msg_t msg;
  if (isMaster) {
    queue = zmq_socket(ctx, ZMQ_PULL);
    assert(0 == zmq_bind(queue, "inproc://0"));
    assert(0 == zmq_msg_init(&msg));
  } else {
    queue = zmq_socket(ctx, ZMQ_PUSH);
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
    open_byte_vl_as_consumer(1, queue, 1);
  } else {
    open_byte_vl_as_producer(1, queue, 1);
  }
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
  double msg[msgSz];
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
  double msg[msgSz];
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
#endif

#endif /* EMBER_INCAST */

  ready++;
  while( nthreads != ready ){ /** spin **/ };

#ifdef EMBER_INCAST
  incast(isMaster, &msg, queue);
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
    } else if (0 == strcmp("-sleep", argv[i])) {
      sleep_nsec = atol(argv[i + 1]);
      ++i;
    } else if (0 == strcmp("-msgSz", argv[i])) {
      msgSz = atoi(argv[i + 1]);
      ++i;
    }
  }
  printf("Px x Py:        %4d x %4d\n", pex, pey);
  printf("Message Size:         %5d\n", msgSz);
  printf("Iterations:           %5d\n", repeats);
  nthreads = pex * pey;
  ready = -1;

#ifdef EMBER_INCAST

#ifdef ZMQ
  ctx = zmq_ctx_new();
  assert(ctx);
#elif BOOST
  boost_queues.resize(1);
  boost_queues[0] = new boost_q_t(msgSz / sizeof(double));
#elif VL
  mkvl(0);
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
    mkvl(0);
  }
#endif

#endif /* EMBER_INCAST */

  pthread_t threads[nthreads];
  int ids[nthreads];
  for (i = 0; nthreads > i; ++i) {
    ids[i] = i;
    pthread_create(&threads[i], NULL, worker, (void *)&ids[i]);
  }
  compute(1000000);

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  ready++;
  for (i = 0; nthreads > i; ++i) {
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
