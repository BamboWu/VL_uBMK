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

int tosort_fd; // master enqueues swap/rswap tasks, slaves dequeue
int topair_fd; // slaves enqueue finished tasks, master dequeues to pair
int *arr_base;
uint64_t arr_len;

std::atomic<int> ready;

#ifdef DBG
std::vector<std::ostringstream> oss;
void dbg(int id) {
  std::cout << "dbg(" << id << ")\n";
  std::cout << oss[id].str() << std::endl;
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
  const uint64_t mini_task_len = 1 << MINI_TASK_EXP;
  bool done = false;;
  vlendpt_t tosort_cons, topair_prod;
  int errorcode;
  Message<int> msg;
  size_t cnt;
  uint64_t task_beg;
  uint8_t task_exp;
  bool loaded = false;

#ifdef INIT_RELOAD
  uint8_t *icnts = new uint8_t[len >> (MINI_TASK_EXP + 1)]();
#endif

  // open endpoints
  if ((errorcode = open_byte_vl_as_consumer(tosort_fd, &tosort_cons, 1))) {
    printf("\033[91mFAILED:\033[0m %s(), tosort_cons\n", __func__);
    return;
  }
  if ((errorcode = open_byte_vl_as_producer(topair_fd, &topair_prod,
          (65536 >> MINI_TASK_EXP) / (NUM_SLAVES + 1)))) {
    printf("\033[91mFAILED:\033[0m %s(), topair_prod\n", __func__);
    return;
  }

#ifdef EXCL_RELOAD
  vlendpt_t tosort_prod;
  if ((errorcode = open_byte_vl_as_producer(tosort_fd, &tosort_prod,
          (65536 >> MINI_TASK_EXP) / (NUM_SLAVES + 1)))) {
    printf("\033[91mFAILED:\033[0m %s(), tosort_prod\n", __func__);
    return;
  }
#endif

  ready++;
  while ((1 + NUM_SLAVES) != ready.load()) { /** spin **/ };

  while (!done) {
    if (!loaded) {
      // try getting a sorting task scheduled
      cnt = MSG_SIZE;
      line_vl_pop_non(&tosort_cons, (uint8_t*)&msg, &cnt);
      if (MSG_SIZE == cnt) { // get a sorting task
        loaded = true;
      }
    }

    if (loaded) {
#if defined(INIT_RELOAD) || defined(EXCL_RELOAD)
      msg.arr.loaded = loaded = false;
#else
      loaded = false;
#endif
      // do the sorting task
      task_beg = msg.arr.beg;
      task_exp = msg.arr.exp;
#ifdef DBG
      oss[desired_core] << "TASK: " <<
        (msg.arr.torswap ? "rswap(" : "swap(") << task_beg <<
        ", " << (uint64_t)task_exp << ")\n";
#endif
      if (MINI_TASK_EXP < task_exp) { // not initial task or bottom
        uint64_t beg_tmp = (task_beg >> task_exp) << task_exp;
        if (msg.arr.torswap) {
          rswap_seg(&arr[beg_tmp], 1 << task_exp,
                    task_beg - beg_tmp, task_beg + mini_task_len - beg_tmp);
#ifdef DBG
          oss[desired_core] << "  rswap_seg(arr[" << beg_tmp << "], " <<
            (1 << task_exp) << ", " << (task_beg - beg_tmp) << ", " <<
            (task_beg + mini_task_len - beg_tmp) << ")\n";
#endif
        } else {
          swap_seg(&arr[beg_tmp], 1 << task_exp,
                   task_beg - beg_tmp, task_beg + mini_task_len - beg_tmp);
#ifdef DBG
          oss[desired_core] << "  swap_seg(arr[" << beg_tmp << "], " <<
            (1 << task_exp) << ", " << (task_beg - beg_tmp) << ", " <<
            (task_beg + mini_task_len - beg_tmp) << ")\n";
#endif
        }
#ifdef EXCL_RELOAD
        // When the slave is the only one doing the task, once done,
        // knows the following depedent tasks are ready to roll
        if ((MINI_TASK_EXP + 1) == task_exp && 1 < desired_core) {
          // if all slaves do EXCL_RELOAD, there could be a dead lock
          msg.arr.exp = MINI_TASK_EXP;
          msg.arr.torswap = false;
          line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
#ifdef DBG
          oss[desired_core] << "  excl_reload_tosort(" << msg.arr.beg <<
            ", " << (uint64_t)msg.arr.exp << ", swap)\n";
#endif
          msg.arr.beg+= mini_task_len;
          msg.arr.loaded = loaded = true;
#ifdef DBG
          oss[desired_core] << "  excl_reload(" << msg.arr.beg << ", " <<
            (uint64_t)msg.arr.exp << ", swap)\n";
#endif
        }
#endif
      } else if (MINI_TASK_EXP == task_exp) { // bottom task
        uint64_t len_tmp = 1 << task_exp;
        uint64_t task_end = task_beg + len_tmp;
        if (msg.arr.torswap) {
          rswap(&arr[task_beg], len_tmp);
#ifdef DBG
          oss[desired_core] << "  rswap(arr[" << task_beg << "], " <<
            len_tmp << ")\n";
#endif
        } else {
          swap(&arr[task_beg], len_tmp);
#ifdef DBG
          oss[desired_core] << "  swap(arr[" << task_beg << "], " <<
            len_tmp << ")\n";
#endif
        }
        // recursive to the bottom
#ifdef DBG
        oss[desired_core] << "  recursive_swap(arr[" << task_beg << "], " <<
          len_tmp << ")\n";
#endif
        while (2 < len_tmp) {
          len_tmp >>= 1;
          for (uint64_t beg_tmp = task_beg; beg_tmp < task_end;
               beg_tmp += len_tmp) {
            swap(&arr[beg_tmp], len_tmp);
          }
        }
      } else { // initial task
        uint8_t exp_tmp = 0; // sorted length
        uint64_t task_end = task_beg + mini_task_len;
        uint64_t task_len, len_tmp;
        if (len < task_end) {
          task_end = len;
        }
        task_len = task_end - task_beg;
        for (uint64_t beg_tmp = task_beg; task_end > beg_tmp; beg_tmp += 2) {
          //swap(&arr[beg_tmp], 2);
          if (arr[beg_tmp] > arr[beg_tmp + 1]) {
            const int tmp = arr[beg_tmp];
            arr[beg_tmp] = arr[beg_tmp + 1];
            arr[beg_tmp + 1] = tmp;
          }
        }
        len_tmp = 1 << (exp_tmp++);
        while (task_len > len_tmp) {
          len_tmp <<= 1;
          // first rswap
          for (uint64_t beg_tmp = task_beg; task_end > beg_tmp;
               beg_tmp += len_tmp) {
            rswap(&arr[beg_tmp], len_tmp);
          }
          // then recursive swap to the bottom
          while (2 < len_tmp) {
            len_tmp >>= 1;
            for (uint64_t beg_tmp = task_beg; beg_tmp < task_end;
                 beg_tmp += len_tmp) {
              swap(&arr[beg_tmp], len_tmp);
            }
          }
          len_tmp = 1 << (exp_tmp++);
        }
#ifdef INIT_RELOAD
        // If the icnts[] indicates this slave has processed another init task
        // can be paired with this one, the slave can schedule two tasks
        uint64_t icnts_idx = task_beg >> (MINI_TASK_EXP + 1);
        if (icnts[icnts_idx] && len > mini_task_len) {
          msg.arr.beg = icnts_idx << (MINI_TASK_EXP + 1);
          msg.arr.exp = MINI_TASK_EXP + 1;
          msg.arr.torswap = true;
          msg.arr.loaded = loaded = true;
#ifdef DBG
          oss[desired_core] << "  init_reload(" << msg.arr.beg << ", " <<
            (uint64_t)msg.arr.exp << ", rswap)\n";
#endif
        } else {
          icnts[icnts_idx]++;
        }
#endif
      }
      line_vl_push_weak(&topair_prod, (uint8_t*)&msg, MSG_SIZE);
      continue;
    } // end of loaded

    // did not have a task to do, flush queue then check the done signal
    line_vl_push_non(&topair_prod, (uint8_t*)&msg, 0);
#ifdef EXCL_RELOAD
    line_vl_push_non(&tosort_prod, (uint8_t*)&msg, 0);
#endif
    done = lock.done;
  }
#ifdef INIT_RELOAD
  delete[] icnts;
#endif
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  setAffinity(0);
  int core_id = 1;
  int errorcode;
  size_t cnt;
  uint64_t task_beg;
  uint8_t task_exp;
  vlendpt_t tosort_prod, topair_cons;

  // make vlinks
  tosort_fd = mkvl();
  if (0 > tosort_fd) {
    printf("\033[91mFAILED:\033[0m tosort_fd = mkvl() "
           "return %d\n", tosort_fd);
    return;
  }
  topair_fd = mkvl();
  if (0 > topair_fd) {
    printf("\033[91mFAILED:\033[0m topair_fd = mkvl() "
           "return %d\n", topair_fd);
    return;
  }

  // open endpoints
  if ((errorcode = open_byte_vl_as_producer(tosort_fd, &tosort_prod,
          (65536 >> MINI_TASK_EXP) / (NUM_SLAVES + 1)))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_producer(tosort_fd) "
           "return %d\n", errorcode);
    return;
  }

  if ((errorcode = open_byte_vl_as_consumer(topair_fd, &topair_cons, 16))) {
    printf("\033[91mFAILED:\033[0m open_byte_vl_as_consumer(topair_fd) "
           "return %d\n", errorcode);
    return;
  }

  // set common info and ready counter before launching slave threads
  arr_base = arr;
  arr_len = len;
  ready = 0;
#ifdef DBG
  oss.emplace_back(std::ostringstream::ate);
#endif
  std::vector<thread> slave_threads;
  for (int i = 0; NUM_SLAVES > i; ++i) {
#ifdef DBG
    oss.emplace_back(std::ostringstream::ate);
#endif
    slave_threads.push_back(thread(slave, core_id++));
  }

  const uint64_t cnt_len = len >> MINI_TASK_EXP;
  uint8_t *scnts = new uint8_t[cnt_len](); // how long has been sorted
  // e.g., scnts[2] = 0 means
  // arr[2<<MINI_TASK_EXP:3<<MINI_TASK_EXP) is in the initial state not sorted
  // e.g., scnts[4] = 2 + MINI_TASK_EXP means
  // arr[4<<MINI_TASK_EXP:8<<MINI_TASK_EXP) is sorted
  uint8_t *dcnts = new uint8_t[cnt_len](); // how long the rswap/swap has done
  uint8_t *bcnts = new uint8_t[cnt_len](); // how long has touch the bottom

  ready++;
  while ((1 + NUM_SLAVES) != ready.load()) { /** spin **/ };

  const uint64_t beg_tsc = rdtsc();
  const auto beg(high_resolution_clock::now());

#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif

  Message<int> msg(arr, len, 0, 2);
  uint64_t feed_in = 0;  // record how long the array has been feed in
  uint64_t on_the_fly = 0;
  msg.arr.exp = 0;
  const uint64_t mini_task_len = 1 << MINI_TASK_EXP;
  for (; len > feed_in;) {
#ifdef DBG
    oss[0] << "init_feedin " << feed_in << "\n";
#endif
    msg.arr.beg = feed_in;
    feed_in += mini_task_len;
    //msg.arr.end = feed_in;
    line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
    if (MAX_ON_THE_FLY <= ++on_the_fly) {
      break;
    }
  }

  bool msg_valid = false;
  while (true) {
    // check if there is any message to pair
    cnt = MSG_SIZE;
    line_vl_pop_non(&topair_cons, (uint8_t*)&msg, &cnt);
    if (MSG_SIZE == cnt) {
#ifdef DBG
      oss[0] << "topair(" << msg.arr.beg << "," << (uint64_t)msg.arr.exp <<
        "," << ((msg.arr.torswap) ? "rswap," : "swap,") <<
        ((msg.arr.loaded) ? "loaded)\n" : "unloaded)\n");
#endif
#if defined(INIT_RELOAD) || defined(EXCL_RELOAD)
      if (msg.arr.loaded) {
        on_the_fly--;
        // update cnts only
#ifdef INIT_RELOAD
        if ((MINI_TASK_EXP + 1) == msg.arr.exp) { // INIT_RELOAD
          uint64_t idx_tmp = msg.arr.beg >> MINI_TASK_EXP;
          scnts[idx_tmp] = scnts[idx_tmp + 1] = MINI_TASK_EXP;
          dcnts[idx_tmp] = 0;
          // reset for the new rswap task tree
          bcnts[idx_tmp] = bcnts[idx_tmp + 1] = 0;
          // reset for the new merge task tree
          on_the_fly += 2;
        }
      } else if (0 == msg.arr.exp && scnts[msg.arr.beg >> MINI_TASK_EXP]) {
          // have seen the loaded init task message first
#endif // end of ifdef INIT_RELOAD
      } else {
        msg_valid = true;
      }
#else // no reload so all messages are not loaded
      msg_valid = true;
#endif
    }

    // process the message if there is a valid one
    if (msg_valid) {
      task_beg = msg.arr.beg;
      task_exp = msg.arr.exp;
      on_the_fly--;
      if (MINI_TASK_EXP < task_exp) { // not bottom of merge tree
        // a swap/rswap task may be split, counting all segements form a tree
        uint8_t cnt = maxdone(dcnts, task_beg >> MINI_TASK_EXP,
                              task_exp - MINI_TASK_EXP);
        if ((cnt + MINI_TASK_EXP) == task_exp) { // swap/rswap len reached
#ifdef DBG
          oss[0] << "  completed(" << ((task_beg >> task_exp) << task_exp) <<
            "," << (uint64_t)task_exp <<
            ((msg.arr.torswap) ? ",rswap)\n" : ",swap)\n");
#endif
          msg.arr.exp = task_exp - 1;
          msg.arr.torswap = false;
          task_beg = (task_beg >> task_exp) << task_exp;
          uint64_t quarter_len = 1 << (task_exp - 2);
          uint64_t task_end = task_beg + quarter_len;
#ifdef DBG
          oss[0] << "  TASK: swap(" << task_beg << ":" << mini_task_len <<
            ":" << task_end << "," << (uint64_t)msg.arr.exp << ")\n";
          oss[0] << "  reset dcnts from " << (task_beg >> MINI_TASK_EXP) <<
            " to " << (task_end >> MINI_TASK_EXP) << "\n";
#endif
          // first swap task
          for (; task_beg < task_end;) {
            dcnts[task_beg >> MINI_TASK_EXP] = 0;
            // reset for the new swap task tree
            msg.arr.beg = task_beg;
            task_beg += mini_task_len;
            msg.arr.end = task_beg;
            line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
            on_the_fly++;
          }
          // second swap task
          task_beg = task_end + quarter_len;
          task_end = task_beg + quarter_len;
#ifdef DBG
          oss[0] << "  TASK: swap(" << task_beg << ":" << mini_task_len <<
            ":" << task_end << "," << (uint64_t)msg.arr.exp << ")\n";
          oss[0] << "  reset dcnts from " << (task_beg >> MINI_TASK_EXP) <<
            " to " << (task_end >> MINI_TASK_EXP) << "\n";
#endif
          for (; task_beg < task_end;) {
            dcnts[task_beg >> MINI_TASK_EXP] = 0;
            // reset for the new swap task tree
            msg.arr.beg = task_beg;
            task_beg += mini_task_len;
            msg.arr.end = task_beg;
            line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
            on_the_fly++;
          }
        }
      } else if (MINI_TASK_EXP == task_exp) { // bottom of merge tree
        // check if the entire merge tree is done
        uint64_t sorted_beg;
        task_exp = maxsorted(scnts, task_beg >> MINI_TASK_EXP, &sorted_beg);
        uint8_t cnt = maxdone(bcnts, task_beg >> MINI_TASK_EXP,
                              ++task_exp - MINI_TASK_EXP + 1);
        if ((cnt + MINI_TASK_EXP - 1) == task_exp) { // merged two sorted
          task_beg = (task_beg >> task_exp) << task_exp;
#ifdef DBG
          oss[0] << "  merged(" << task_beg << ":" <<
            (task_beg + (1 << task_exp)) << ")\n";
#endif
          uint64_t idx_tmp = task_beg >> MINI_TASK_EXP;
          scnts[idx_tmp]++; // update the maximum sorted array length
          // check whether the entire array has been sorted or not
          if ((uint64_t)(1 << scnts[0]) == len) {
            break;
          }
          // try pairing the new sorted array with adjacent sorted array
          if (ispaired(scnts, idx_tmp, &idx_tmp)) {
            msg.arr.exp = task_exp + 1;
            msg.arr.torswap = true;
            task_beg = idx_tmp << MINI_TASK_EXP;
#ifdef DBG
            oss[0] << "    paired(" << task_beg << "," << (task_exp + 1) <<
              ")\n";
#endif
            uint64_t half_len = 1 << (task_exp - MINI_TASK_EXP);
            uint64_t idx_end = idx_tmp + half_len;
#ifdef DBG
            oss[0] << "    TASK: rswap(" << task_beg << ":" << mini_task_len <<
              ":" << (idx_end << MINI_TASK_EXP) << "," <<
              (uint64_t)msg.arr.exp << ")\n";
            oss[0] << "    reset d/bcnts from " << (uint64_t)idx_tmp;
#endif
            for (; idx_tmp < idx_end;) {
              dcnts[idx_tmp] = 0;
              // reset for the new rswap task tree
              bcnts[idx_tmp] = 0;
              // reset for the new merge task tree
              msg.arr.beg = task_beg;
              idx_tmp++;
              task_beg = idx_tmp << MINI_TASK_EXP;
              msg.arr.end = task_beg;
              line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
              on_the_fly++;
            }
            idx_end = idx_tmp + half_len;
#ifdef DBG
            oss[0] << " to " << (uint64_t)idx_tmp << "/" <<
              (uint64_t)idx_end << "\n";
#endif
            for (; idx_tmp < idx_end; ++idx_tmp) {
              bcnts[idx_tmp] = 0;
              // reset for the new merge task tree
            }
          } // end of ispaired
        } // end of merged two sorted arrays
      } else { // 0 == task_exp, only the initial tasks could
        uint64_t idx_tmp = task_beg >> MINI_TASK_EXP;
        scnts[idx_tmp] = MINI_TASK_EXP;
        // check whether the entire array has been sorted or not
        if ((uint64_t)(1 << scnts[0]) >= len) {
          break;
        }
#ifdef DBG
        oss[0] << "  inited(" << task_beg << ")\n";
#endif
        // try pairing the new sorted array with adjacent sorted array
        if (ispaired(scnts, idx_tmp, &idx_tmp)) {
          msg.arr.exp = MINI_TASK_EXP + 1;
          msg.arr.torswap = true;
          task_beg = idx_tmp << MINI_TASK_EXP;
#ifdef DBG
          oss[0] << "    paired(" << task_beg << ")\n";
          oss[0] << "    TASK rswap(" << task_beg << "," <<
            (uint64_t)msg.arr.exp << ")\n";
          oss[0] << "    reset dcnts[" << (uint64_t)idx_tmp << "], bcnts[" <<
            (uint64_t)idx_tmp << ":" << (uint64_t)(idx_tmp + 1) << "]\n";
#endif
          dcnts[idx_tmp] = 0;
          // reset for the new rswap task tree
          bcnts[idx_tmp] = bcnts[idx_tmp + 1] = 0;
          // reset for the new merge task tree
          msg.arr.beg = task_beg;
          msg.arr.end = task_beg + mini_task_len;
          line_vl_push_weak(&tosort_prod, (uint8_t*)&msg, MSG_SIZE);
          on_the_fly++;
        } // end of ispaired
      }
      msg_valid = false;
      continue;
    } // end of if msg_valid

    // did not process a valid message
    // remaining initial tasks first then flushing queue
    if (len > feed_in && MAX_ON_THE_FLY > on_the_fly) {
      msg.arr.exp = 0;
      msg.arr.beg = feed_in;
      //msg.arr.end = feed_in + mini_task_len;
      if (line_vl_push_non(&tosort_prod, (uint8_t*)&msg, MSG_SIZE)) {
#ifdef DBG
        oss[0] << "feedin " << feed_in << "\n";
#endif
        feed_in += mini_task_len;
        on_the_fly++;
      }
    } else {
      line_vl_push_non(&tosort_prod, (uint8_t*)&msg, 0);
    }
  } // while (true)

  lock.done = true; // tell other worker threads we are done

#ifndef NOGEM5
  m5_dump_reset_stats(0, 0);
#endif

  const uint64_t end_tsc = rdtsc();
  const auto end(high_resolution_clock::now());
  const auto elapsed(duration_cast<nanoseconds>(end - beg));

  std::cout << (end_tsc - beg_tsc) << " ticks elapsed\n";
  std::cout << elapsed.count() << " ns elapsed\n";

  delete[] scnts;
  delete[] dcnts;
  delete[] bcnts;

  for (int i = NUM_SLAVES - 1; 0 <= i; --i) {
    slave_threads[i].join();
  }
#ifdef DBG
  for (int i = 0; NUM_SLAVES >= i; ++i) {
    dbg(i);
  }
  dump(arr, 0); // just to instantiate dump<int>
#endif
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
  std::cout << std::endl;
  check(arr, len);
  free(arr);
  return 0;
}
