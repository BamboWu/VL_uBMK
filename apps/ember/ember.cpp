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

void compute(long sleep) {
  struct timespec sleepTS;
  struct timespec remainTS;
  sleepTS.tv_sec = 0;
  sleepTS.tv_nsec = sleep;
  if (EINTR == nanosleep(&sleepTS, &remainTS)) {
    while (EINTR == nanosleep(&remainTS, &remainTS));
  }
}

/* Global variables */
int pex, pey, nthreads;
int repeats;
int msgSz;
long sleep;
std::atomic< int > ready;
#ifdef ZMQ
void *ctx;
#endif

void sweep(const int xUp, const int xDn, const int yUp, const int yDn,
           double *xRecvBuffer, double *xSendBuffer,
           double *yRecvBuffer, double *ySendBuffer,
#ifdef ZMQ
           void *xUpSend, void *xDnSend, void *yUpSend, void *yDnSend,
           void *xUpRecv, void *xDnRecv, void *yUpRecv, void *yDnRecv) {
#elif VL
           vlendpt_t *xUpSend, vlendpt_t *xDnSend, vlendpt_t *yUpSend,
           vlendpt_t *yDnSend, vlendpt_t *xUpRecv, vlendpt_t *xDnRecv,
           vlendpt_t *yUpRecv, vlendpt_t *yDnRecv) {
#endif

  int i;
#ifdef VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt;
#endif
  for (i = 0; repeats > i; ++i) {
    /* Sweep from (0,0) to (Px,Py) */
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xDnRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yDnRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    compute(sleep);
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xUpSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yUpSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }

    /* Sweep from (Px,0) to (0,Py) */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xUpRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yDnRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    compute(sleep);
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xDnSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yUpSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }

    /* Sweep from (Px,Py) to (0,0) */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xUpRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yUpRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    compute(sleep);
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xDnSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yDnSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }

    /* Sweep from (0,Py) to (Px,0) */
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xDnRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yUpRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    compute(sleep);
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xUpSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yDnSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
  }

}

void halo(const int xUp, const int xDn, const int yUp, const int yDn,
          double *xRecvBuffer, double *xSendBuffer,
          double *yRecvBuffer, double *ySendBuffer,
#ifdef ZMQ
          void *xUpSend, void *xDnSend, void *yUpSend, void *yDnSend,
          void *xUpRecv, void *xDnRecv, void *yUpRecv, void *yDnRecv) {
#elif VL
          vlendpt_t *xUpSend, vlendpt_t *xDnSend, vlendpt_t *yUpSend,
          vlendpt_t *yDnSend, vlendpt_t *xUpRecv, vlendpt_t *xDnRecv,
          vlendpt_t *yUpRecv, vlendpt_t *yDnRecv) {
#endif

  int i;
#if VL
  const int nblks = (msgSz + 55) / 56;
  char buf[64];
  uint16_t *blkId = (uint16_t*)buf; /* used to reorder cache blocks */
  uint16_t idx;
  size_t cnt;
#endif
  for (i = 0; repeats > i; ++i) {
    compute(sleep);

    /* send to four neighbours */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xUpSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(xDnSend, (void*)xSendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&xSendBuffer[7*idx], cnt);
        line_vl_push_strong(xDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yUpSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yUpSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_send(yDnSend, (void*)ySendBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        *blkId = idx;
        cnt = ((nblks - 1) > idx ? 7 : nblks % 7) * sizeof(double);
        memcpy((void*)&buf[sizeof(uint16_t)], (void*)&ySendBuffer[7*idx], cnt);
        line_vl_push_strong(yDnSend, (uint8_t*)buf, cnt + sizeof(uint16_t));
      }
#endif
    }

    /* receive from four neighbors */
    if (-1 < xUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xUpRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < xDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(xDnRecv, (void*)xRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(xDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&xRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yUp) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yUpRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yUpRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
    if (-1 < yDn) {
#ifdef ZMQ
      assert(msgSz == zmq_recv(yDnRecv, (void*)yRecvBuffer, msgSz, 0));
#elif VL
      for (idx = 0; nblks > idx; ++idx) {
        line_vl_pop_weak(yDnRecv, (uint8_t*)buf, &cnt);
        memcpy((void*)&yRecvBuffer[7*(*blkId)],
               (void*)&buf[sizeof(uint16_t)],
               ((nblks - 1) > *blkId ? 7 : nblks % 7) *sizeof(double));
      }
#endif
    }
  }
}

void *worker(void *arg) {
  int *pid = (int*) arg;
  int x, y, xUp, xDn, yUp, yDn;
  int xUpRx, xUpTx, xDnRx, xDnTx, yUpRx, yUpTx, yDnRx, yDnTx;
  setAffinity(*pid);
  get_position(*pid, pex, pey, &x, &y);
  get_neighbor(*pid, pex, pey, x, y, &xUp, &xDn, &yUp, &yDn);
  get_queue_id(x, y, pex, pey, &xUpRx, &xUpTx, &xDnRx, &xDnTx,
               &yUpRx, &yUpTx, &yDnRx, &yDnTx);
  double *xRecvBuffer = (double*)malloc(msgSz);
  double *xSendBuffer = (double*)malloc(msgSz);
  double *yRecvBuffer = (double*)malloc(msgSz);
  double *ySendBuffer = (double*)malloc(msgSz);
#ifdef ZMQ
  void *xUpSend = zmq_socket(ctx, ZMQ_PUSH);
  void *xDnSend = zmq_socket(ctx, ZMQ_PUSH);
  void *yUpSend = zmq_socket(ctx, ZMQ_PUSH);
  void *yDnSend = zmq_socket(ctx, ZMQ_PUSH);
  void *xUpRecv = zmq_socket(ctx, ZMQ_PULL);
  void *xDnRecv = zmq_socket(ctx, ZMQ_PULL);
  void *yUpRecv = zmq_socket(ctx, ZMQ_PULL);
  void *yDnRecv = zmq_socket(ctx, ZMQ_PULL);
  zmq_msg_t msg;
  char queue_str[64];
  assert(0 == zmq_msg_init_size(&msg, msgSz));
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
#elif VL
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

  ready++;
  while( nthreads != ready ){ /** spin **/ };

#ifdef EMBER_SWEEP2D
  sweep(xUp, xDn, yUp, yDn, xRecvBuffer, xSendBuffer, yRecvBuffer, ySendBuffer,
       xUpSend, xDnSend, yUpSend, yDnSend, xUpRecv, xDnRecv, yUpRecv, yDnRecv);
#elif EMBER_HALO2D
  halo(xUp, xDn, yUp, yDn, xRecvBuffer, xSendBuffer, yRecvBuffer, ySendBuffer,
       xUpSend, xDnSend, yUpSend, yDnSend, xUpRecv, xDnRecv, yUpRecv, yDnRecv);
#endif

  /* comment out on purpose to exclude this from ROI.
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
  sleep = 1000;
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
      sleep = atol(argv[i + 1]);
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
#ifdef ZMQ
  ctx = zmq_ctx_new();
  assert(ctx);
#elif VL
  for (i = 0; (pex * pey * 4) - (pex + pey) * 2 > i; ++i) {
    mkvl(0);
  }
#endif
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
