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

#define initialize_lock(lock, threads) vlink_lock_init(lock, threads)

#include "vl/vl.h"

__thread vlendpt_t prod, cons;
__thread bool opened = false;

void vlink_lock_init(uint64_t *lock, uint64_t threads) {
	*lock = mkvl(0);
  vlendpt_t endpt;
  open_byte_vl_as_producer(*lock, &endpt, 1);
  byte_vl_push_non(&endpt, 0xFF);
  byte_vl_flush(&endpt);
}

static inline unsigned long lock_acquire (uint64_t *lock, unsigned long threadnum) {
  uint8_t tmp = 0;
  if (!opened) {
    open_byte_vl_as_producer(*lock, &prod, 1);
    open_byte_vl_as_consumer(*lock, &cons, 1);
    opened = true;
  }
  byte_vl_pop_weak(&cons, &tmp);
  return 1;
}

static inline void lock_release (uint64_t *lock, unsigned long threadnum) {
  byte_vl_push_non(&prod, (uint8_t)threadnum);
  byte_vl_flush(&prod);
}
