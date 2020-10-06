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
#ifdef thread_local_done
#undef thread_local_done
#endif

#define initialize_lock(lock, threads) vlink_lock_init(lock, threads)
#define thread_local_init(smtid) vlink_lock_thread_init(smtid)
#define thread_local_done(smtid) vlink_lock_thread_done(smtid)

#include <signal.h>
#include "vl/vl.h"

volatile sig_atomic_t ready = 0;
__thread vlendpt_t prod, cons;

void vlink_lock_init(uint64_t *lock, uint64_t threads) {
  ready = threads;
	*lock = mkvl(0);
  vlendpt_t endpt;
  open_byte_vl_as_producer(*lock, &endpt, 1);
  byte_vl_push_non(&endpt, 0xFF);
  byte_vl_flush(&endpt);
}

void vlink_lock_thread_init(uint64_t smtid) {
  open_byte_vl_as_producer(1, &prod, 1);
  open_byte_vl_as_consumer(1, &cons, 1);
  ready--;
  while(ready) { /* waiting for all threads get their endpoint ready */ }
}

void vlink_lock_thread_done(uint64_t smtid) {
  close_byte_vl_as_producer(prod);
  close_byte_vl_as_consumer(cons);
}

static inline unsigned long lock_acquire (uint64_t *lock, unsigned long threadnum) {
  uint8_t tmp = 0;
  byte_vl_pop_weak(&cons, &tmp);
  return 1;
}

static inline void lock_release (uint64_t *lock, unsigned long threadnum) {
  byte_vl_push_non(&prod, (uint8_t)threadnum);
  byte_vl_flush(&prod);
}
