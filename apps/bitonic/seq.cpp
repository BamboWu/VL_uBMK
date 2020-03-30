#include <iostream>
#include <queue>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <limits.h>
#include <assert.h>

union Message { // Cacheline-size message
  struct {
    int *base;
    uint64_t len;
    uint64_t beg;
    uint64_t end;
  } arr;
  uint8_t pad[64]; // make sure it is cacheline-size message
  Message(int *base, uint64_t len, uint64_t beg, uint64_t end) {
    arr.base = base;
    arr.len = len;
    arr.beg = beg;
    arr.end = end;
  }
  Message(Message &rhs) {
    arr.base = rhs.arr.base;
    arr.len = rhs.arr.len;
    arr.beg = rhs.arr.beg;
    arr.end = rhs.arr.end;
  }
} __attribute__((packed, aligned(64)));

std::queue<Message> to_pair; // sorted subarray
std::queue<Message> to_reverse; // 2nd sorted subarry to reverse
std::queue<Message> to_swap; // bitonic subarries
std::queue<Message> to_connect; // ordered sorted subarries to be connected

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

/* Check if the array is ascending */
void check(const int *arr, const uint64_t len) {
  for (uint64_t i = 1; len > i; ++i) {
    if (arr[i - 1] > arr[i]) {
      std::cout << "\033[91mERROR: arr[" << (i - 1) << "] = " << arr[i - 1] <<
        " > arr[" << i << "] = " << arr[i] << "\033[0m\n";
      break;
    }
  }
}

/* Dump an array */
void dump(const int *arr, const uint64_t len) {
  std::cout << arr[0];
  for (uint64_t i = 1; len > i; ++i) {
    std::cout << ", " << arr[i];
  }
}

#ifdef DBG
void print(const int *arr, const uint64_t len, const uint64_t beg,
           const uint64_t end, const char *prefix, const char *suffix) {
  if (0 < beg) {
    dump(arr, beg);
    std::cout << ", ";
  }
  std::cout << prefix;
  dump(&arr[beg], end - beg);
  std::cout << suffix;
  if (len > end) {
    std::cout << ", ";
    dump(&arr[end], len - end);
  }
  std::cout << std::endl;
}
#endif

/* Swap values in the first half of a bitonic with corresponding values
 * in the later half if the value from the first half is bigger */
void swap(int *arr, const uint64_t len) {
  const uint64_t half = len >> 1;
  for (uint64_t i = 0; half > i; ++i) {
    const uint64_t pair = i + half;
    if (arr[i] > arr[pair]) {
      const int tmp = arr[i];
      arr[i] = arr[pair];
      arr[pair] = tmp;
    }
  }
}

/* Reverse an array */
void reverse(int *arr, const uint64_t len) {
  const uint64_t half = len >> 1;
  for (uint64_t i = 0; half > i; ++i) {
    const uint64_t pair = len - i - 1;
    const int tmp = arr[i];
    arr[i] = arr[pair];
    arr[pair] = tmp;
  }
}

/* Fill an array with a certain number */
void fill(int *arr, const uint64_t len, const int val) {
#ifdef DBG
  std::cout << "fill(0x" << std::hex << (uint64_t)arr <<
    std::dec << ", " << len << ", " << val << ")\n";
#endif
  for (uint64_t i = 0; len > i; ++i) {
    arr[i] = val;
  }
}

/* Generate an unsorted array filled with random numbers */
void gen(int *arr, const uint64_t len) {
#ifdef DBG
  std::cout << "gen(0x" << std::hex << (uint64_t)arr <<
    std::dec << ", " << len << ")\n";
#endif
  srand(2699);
  for (uint64_t i = 0; len > i; ++i) {
    arr[i] = rand();
  }
}

/* Sort an array */
void sort(int *arr, const uint64_t len) {
  uint8_t *ccount = new uint8_t[len](); // count number of entering connect()
  uint8_t *pcount = new uint8_t[len](); // count number of entering pair()

  // every two elements form a biotonic subarray, ready for swap
  for (uint64_t i = 0; len > i; i += 2) {
    to_swap.emplace(arr, len, i, i + 2);
  }

  while (true) {
    /* what a group of threads do to swap */
    if (!to_swap.empty()) {
      Message msg(to_swap.front());
      to_swap.pop();
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      uint64_t len_tmp = msg.arr.end - msg.arr.beg;
#ifdef DBG
      std::cout << "swap(&arr[" << msg.arr.beg << "], " << len_tmp << ")\n";
      if (16 >= msg.arr.len) {
        dump(msg.arr.base, msg.arr.len);
        std::cout << std::endl;
      }
#endif
      swap(arr_tmp, len_tmp);
#ifdef DBG
      if (16 >= msg.arr.len) {
        print(msg.arr.base, msg.arr.len, msg.arr.beg, msg.arr.end,
              "\033[43;97m", "\033[0m");
      }
#endif
      if (2 < len_tmp) {
        len_tmp >>= 1;
        msg.arr.end = msg.arr.beg + len_tmp;
        to_swap.emplace(msg);
        msg.arr.beg += len_tmp;
        msg.arr.end += len_tmp;
        to_swap.emplace(msg);
      } else {
        to_connect.emplace(msg);
      }
    }

    /* what a thread do to connect */
    if (!to_connect.empty()) {
      Message msg(to_connect.front());
      to_connect.pop();
      const uint64_t beg_tmp = msg.arr.beg;
      ccount[beg_tmp]++;
      const uint64_t len_to_connect = 1 << ccount[beg_tmp];
      const uint64_t idx_beg = beg_tmp & ~(len_to_connect - 1);
      const uint64_t idx_end = beg_tmp | (len_to_connect - 1);
#ifdef DBG
      std::cout << "connect(&arr[" << beg_tmp << "], " << len_to_connect <<
        ") checks [" << idx_beg << ":2:" << idx_end << "]\n";
#endif
      bool connected = true;
      for (uint64_t i = idx_beg; idx_end > i; i += 2) {
        if (ccount[i] != ccount[beg_tmp]) {
          connected = false;
          break;
        }
      }
      if (connected) {
#ifdef DBG
        std::cout << " \033[32mconnected [" << idx_beg << ":" <<
          (idx_end + 1) << ")\033[0m\n";
#endif
        msg.arr.beg = idx_beg;
        msg.arr.end = idx_end + 1;
        to_pair.emplace(msg);
      }
    }

    /* what a thread do to pair */
    if (!to_pair.empty()) {
      Message msg(to_pair.front());
      to_pair.pop();
      const uint64_t beg_tmp = msg.arr.beg;
      pcount[beg_tmp]++;
      const uint64_t stride_to_pair = msg.arr.end - beg_tmp;
      const uint64_t idx_1st = beg_tmp & ~((stride_to_pair << 1) - 1);
      const uint64_t idx_2nd = idx_1st + stride_to_pair;
#ifdef DBG
      std::cout << "pair(&arr[" << beg_tmp << "], " << stride_to_pair <<
        ") checks [" << idx_1st << ":] and [" << idx_2nd << ":]\n";
#endif
      if (stride_to_pair == msg.arr.len) {
#ifdef DBG
        std::cout << " \033[92mentire array is sorted\033[0m\n";
#endif
        break;
      } else if (pcount[idx_1st] == pcount[idx_2nd]) {
#ifdef DBG
        std::cout << " \033[36mpaired [" << idx_1st << ":" <<
          (idx_2nd + stride_to_pair) << ")\033[0m\n";
#endif
        msg.arr.beg = idx_1st;
        msg.arr.end = idx_2nd;
        to_reverse.emplace(msg);
      }
    }

    /* what a group of threads do to reverse */
    if (!to_reverse.empty()) {
      Message msg(to_reverse.front());
      to_reverse.pop();
      int *arr_tmp = &msg.arr.base[msg.arr.beg];
      const uint64_t len_tmp = msg.arr.end - msg.arr.beg;
#ifdef DBG
      std::cout << "reverse(&arr[" << msg.arr.beg << "], " << len_tmp << ")\n";
      if (16 >= msg.arr.len) {
        dump(msg.arr.base, msg.arr.len);
        std::cout << std::endl;
      }
#endif
      reverse(arr_tmp, len_tmp);
#ifdef DBG
      if (16 >= msg.arr.len) {
        print(msg.arr.base, msg.arr.len, msg.arr.beg, msg.arr.end,
              "\033[44;97m", "\033[0m");
      }
#endif
      msg.arr.end += len_tmp;
      to_swap.emplace(msg);
    }
  }
  delete[] ccount;
  delete[] pcount;
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
  dump(arr, len);
  std::cout << std::endl;
  check(arr, len);
  free(arr);
  return 0;
}
