#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>
#include <atomic>
#include <sstream>

#ifndef STDTHREAD
#include <boost/thread.hpp>
#else
#include <thread>
#endif

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;

#include "threading.h"
#include "timing.h"
#include "utils.hpp"

#include "vl/vl.h"

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

#ifndef STDTHREAD
using boost::thread;
#else
using std::thread;
#endif

int topair_fd;
int toslave_fd;
int tomaster_fd;
int *arr_base;
uint64_t arr_len;

std::atomic<int> ready;

#ifdef DBG
std::ostringstream oss (std::ostringstream::ate);
void dbg() {
  std::cout << oss.str();
}
#endif

union {
  bool done; // to tell other threads we are done, only master thread writes
  char pad[64];
} volatile __attribute__((aligned(64))) lock = { .done = false };

uint64_t roundup64(const uint64_t val) {
  uint64_t val64 = val - 1;
  val64 |= (val64 >> 1);
  val64 |= (val64 >> 2);
  val64 |= (val64 >> 4);
  val64 |= (val64 >> 8);
  val64 |= (val64 >> 16);
  val64 |= (val64 >> 32);
  return ++val64;
}

void slave(const int desired_core) {
  setAffinity(desired_core);
  int *arr = arr_base;
  const uint64_t len = arr_len;
  uint8_t *pcount = new uint8_t[len](); // count number of pairing
  bool done = false;;
  vlendpt_t toslave_cons, topair_prod, tomaster_prod;
  int errorcode;
  Message<int> msg;
  size_t cnt;
  uint64_t task_beg;
  bool loaded = false;
  // loaded indicates if task_beg, pcount[task_beg] are set for a valid task

  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(toslave_fd, &toslave_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), toslave_cons\n", __func__);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(topair_fd, &topair_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), topair_prod\n", __func__);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(tomaster_fd, &tomaster_prod, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), tomaster_prod\n", __func__);
    return;
  }

#ifdef SLAVE_HELP_PAIRING
  vlendpt_t topair_cons;
  if ((errorcode = open_byte_vl_as_consumer(topair_fd, &topair_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), topair_cons\n", __func__);
    return;
  }
#endif

  ready++;
  while ((1 + NUM_SLAVES) != ready) { /** spin **/ };

  while (!done) {
    if (!loaded) {
      // try getting a sorting task scheduled
      line_vl_pop_non(&toslave_cons, (uint8_t*)&msg, &cnt);
      if (MSG_SIZE == cnt) { // get a sorting task
        task_beg = msg.arr.beg;
        pcount[task_beg] = msg.arr.cnt;
        loaded = true;
      }
    }

    if (loaded) {
      // do the sorting task
      pcount[task_beg]++;
      uint64_t len_tmp = 1 << pcount[task_beg];
      uint64_t task_end = task_beg + len_tmp;
      loaded = false;
      rswap(&arr[task_beg], len_tmp);
      while (2 < len_tmp) {
        len_tmp >>= 1;
        for (uint64_t beg_tmp = task_beg; beg_tmp < task_end;
             beg_tmp += len_tmp) {
          swap(&arr[beg_tmp], len_tmp);
        }
      }
      if (len == (uint64_t)(1 << pcount[0])) {
        msg.arr.beg = 0;
        msg.arr.cnt = pcount[0];
        line_vl_push_strong(&tomaster_prod, (uint8_t*)&msg, MSG_SIZE);
        break; // we are done
      }

#ifdef SLAVE_LOAD_SELF
      // do pairing before sending out
      uint64_t beg_tmp;
      loaded = pair(pcount, task_beg, &beg_tmp);
      task_beg = loaded ? beg_tmp : task_beg;
#endif // SLAVE_LOAD_SELF

      msg.arr.beg = task_beg;
      msg.arr.cnt = pcount[task_beg];
      msg.arr.loaded = loaded;
      if (loaded) { // paired, take the task and just let the master know
        line_vl_push_strong(&tomaster_prod, (uint8_t*)&msg, MSG_SIZE);
      } else { // need other slaves or the master to pair
        line_vl_push_strong(&topair_prod, (uint8_t*)&msg, MSG_SIZE);
      }
      continue;
    }

#ifdef SLAVE_HELP_PAIRING
    // try helping the master to schedule a sorting task
    line_vl_pop_non(&topair_cons, (uint8_t*)&msg, &cnt);
    if (MSG_SIZE == cnt) { // get a sorting task
      if (msg.arr.cnt > pcount[msg.arr.beg]) {
        pcount[msg.arr.beg] = msg.arr.cnt; // could only increase
        loaded = pair(pcount, msg.arr.beg, &task_beg);
        if (task_beg != msg.arr.beg) { // break symmetry, otherwise 2 slaves
            loaded = false;            // could claim the same task
        }
        msg.arr.loaded = loaded;
      }
      // no matter paired or not, let the master know
      line_vl_push_strong(&tomaster_prod, (uint8_t*)&msg, MSG_SIZE);
    }
#endif
    done = lock.done;
  }
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  int errorcode;
  size_t cnt;
  uint64_t task_beg;
  vlendpt_t topair_cons, toslave_prod, tomaster_cons;

  // make vlinks
  topair_fd = mkvl();
  if (0 > topair_fd) {
    printf("\033[91mFAILED:\033[0m topair_fd = mkvl() "
           "return %d\n", topair_fd);
    return;
  }
  toslave_fd = mkvl();
  if (0 > toslave_fd) {
    printf("\033[91mFAILED:\033[0m toslave_fd = mkvl() "
           "return %d\n", toslave_fd);
    return;
  }
  tomaster_fd = mkvl();
  if (0 > tomaster_fd) {
    printf("\033[91mFAILED:\033[0m tomaster_fd = mkvl() "
           "return %d\n", tomaster_fd);
    return;
  }

  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(topair_fd, &topair_cons, 1))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_producer(toslave_fd) "
           "return %d\n", errorcode);
    return;
  }

  if ((errorcode = open_byte_vl_as_producer(toslave_fd, &toslave_prod, 1))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_producer(toslave_fd) "
           "return %d\n", errorcode);
    return;
  }

  if ((errorcode = open_byte_vl_as_consumer(tomaster_fd, &tomaster_cons, 1))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_consumer(tomaster_fd) "
           "return %d\n", errorcode);
    return;
  }

  // set common info and ready counter before launchgin slave threads
  arr_base = arr;
  arr_len = len;
  ready = 0;
  std::vector<thread> slave_threads;
  for (int i = 0; NUM_SLAVES > i; ++i) {
    slave_threads.push_back(thread(slave, core_id++));
  }
  ready++;
  while ((1 + NUM_SLAVES) != ready) { /** spin **/ };

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  // every two elements form a biotonic subarray, ready for swap
  Message<int> msg(arr, len, 0, 2);
  uint64_t feed_in = 0;  // record how long the array has been feed in
  for (; len > feed_in; feed_in += 2) {
    msg.arr.beg = feed_in;
    msg.arr.cnt = 0;
    if (!line_vl_push_non(&toslave_prod, (uint8_t*)&msg, MSG_SIZE) ||
        MAX_ON_THE_FLY <= vl_size(&toslave_prod)) {
      // reach the queue capacity, do the remaining later
      break;
    }
#ifdef DBG
    oss << "init_feedin " << feed_in << "\n";
#endif
  }

  uint8_t *pcount = new uint8_t[len](); // count number of pairing
  uint8_t *lcount = new uint8_t[len](); // count number received as loaded
  while (true) {
    // first check if there is any task exclusively for the master
    line_vl_pop_non(&tomaster_cons, (uint8_t*)&msg, &cnt);
    if (MSG_SIZE == cnt) {
      task_beg = msg.arr.beg;
      if (msg.arr.cnt > pcount[task_beg]) {
        pcount[task_beg] = msg.arr.cnt; // could only increase
#ifdef DBG
        oss << "tomaster(pcount[" << task_beg << "]=" <<
            (uint64_t)pcount[task_beg] << "," <<
            ((msg.arr.loaded) ? "loaded)\n" : "unloaded)\n");
#endif
        if (len == (uint64_t)(1 << pcount[0])) {
          break; // we are done
        } else if (!msg.arr.loaded) {
          if (pair(pcount, task_beg, &task_beg) &&         // paired and
                   pcount[task_beg] != lcount[task_beg]) { // haven't taken
            msg.arr.beg = task_beg;
            msg.arr.cnt = pcount[task_beg];
#ifdef DBG
            oss << "toslave(pcount[" << task_beg << "]=" <<
                (uint64_t)pcount[task_beg] << ")\n";
#endif
            line_vl_push_strong(&toslave_prod, (uint8_t*)&msg, MSG_SIZE);
          }
        } else { // it is just a report of taken task
          lcount[task_beg] = msg.arr.cnt;
        }
      } // else { // this is an out-of-dated msg
    } else { // tomaster queue empty, feed in remaining or check topair queue
      if (len > feed_in && MAX_ON_THE_FLY > vl_size(&toslave_prod)) {
        msg.arr.beg = feed_in;
        msg.arr.cnt = 0;
#ifdef DBG
        oss << "feedin " << feed_in << "\n";
#endif
        line_vl_push_strong(&toslave_prod, (uint8_t*)&msg, MSG_SIZE);
        feed_in += 2;
      } else {
        // no remaining array or capacity, check topair queue
        line_vl_pop_non(&topair_cons, (uint8_t*)&msg, &cnt);
        if (MSG_SIZE == cnt) {
          task_beg = msg.arr.beg;
          if (msg.arr.cnt > pcount[task_beg]) {
            pcount[task_beg] = msg.arr.cnt; // could only increase
#ifdef DBG
            oss << "topair(pcount[" << task_beg << "]=" <<
                (uint64_t)pcount[task_beg] << ")\n";
#endif
            if (pair(pcount, task_beg, &task_beg) &&    // paired and
                pcount[task_beg] != lcount[task_beg]) { // haven't taken
              msg.arr.beg = task_beg;
              msg.arr.cnt = pcount[task_beg];
#ifdef DBG
              oss << "toslave(pcount[" << task_beg << "]=" <<
                  (uint64_t)pcount[task_beg] << ")\n";
#endif
              line_vl_push_strong(&toslave_prod, (uint8_t*)&msg, MSG_SIZE);
            }
          }
        }
      }
    }
  } // while (true)

  lock.done = true; // tell other worker threads we are done
  delete[] pcount;

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

#ifdef DBG
  dbg();
#endif
  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  for (int i = NUM_SLAVES - 1; 0 <= i; --i) {
    slave_threads[i].join();
  }
}

int main(int argc, char *argv[]) {
  uint64_t len = 16;
  if (1 < argc) {
    len = strtoull(argv[1], NULL, 0);
  }
  const uint64_t len_roundup = roundup64(len);
  int *arr = (int*) memalign(64, len_roundup * sizeof(int));
  gen(arr, len);
  fill(&arr[len], len_roundup - len, INT_MAX); // padding
  sort(arr, len_roundup);
  //dump(arr, len);
  std::cout << std::endl;
  check(arr, len);
  free(arr);
  return 0;
}
