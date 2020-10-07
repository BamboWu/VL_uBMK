/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * SPDX-License-Identifier:    BSD-3-Clause
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef initialize_lock
#undef initialize_lock
#endif
#ifdef thread_local_init
#undef thread_local_init
#endif

#define initialize_lock(lock, threads) zmq_qlock_init(lock, threads)
#define thread_local_init(smtid) zmq_lock_thread_init(smtid)

#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include "zmq.h"

volatile sig_atomic_t ready = 0;
void *ctx;
void *frontend;
void *backend;
__thread void *prod;
__thread void *cons;

static void *proxy_thread (void *args) {
  //  Start the proxy
  zmq_proxy (frontend, backend, NULL);
  return NULL;
}

void zmq_qlock_init (uint64_t *lock, uint64_t threads) {
  void *firstprod;
  ready = threads;
  ctx = zmq_ctx_new();
  //  Socket facing clients
  frontend = zmq_socket (ctx, ZMQ_ROUTER);
  assert(0 == zmq_bind (frontend, "inproc://router"));
  //  Socket facing services
  backend = zmq_socket (ctx, ZMQ_DEALER);
  assert(0 == zmq_bind (backend, "inproc://dealer"));
  pthread_t pxy_thd;
  pthread_create(&pxy_thd, NULL, proxy_thread, NULL);
  firstprod = zmq_socket (ctx, ZMQ_PUSH);
  assert(0 == zmq_connect (firstprod, "inproc://router"));
  assert (1 == zmq_send(firstprod, "0", 1, 0));
}

void zmq_lock_thread_init(uint64_t smtid) {
  prod = zmq_socket (ctx, ZMQ_PUSH);
  cons = zmq_socket (ctx, ZMQ_PULL);
  assert(0 == zmq_connect (prod, "inproc://router"));
  assert(0 == zmq_connect (cons, "inproc://dealer"));
  ready--;
  while(ready) { /* waiting for all threads get their endpoint ready */ }
}

static inline unsigned long lock_acquire (uint64_t *lock, unsigned long threadnum) {
  char buf;
  assert (-1 != zmq_recv(cons, &buf, 1, 0));
  return 1;
}

static inline void lock_release (uint64_t *lock, unsigned long threadnum) {
  assert (1 == zmq_send(prod, "1", 1, 0));
}
