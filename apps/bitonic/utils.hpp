#ifndef _BITONIC_UTILS_HPP__
#define _BITONIC_UTILS_HPP__

#ifndef MAX_ON_THE_FLY
#define MAX_ON_THE_FLY 128
#endif

#ifndef NUM_SLAVES
#define NUM_SLAVES 1
#endif

template <typename T>
union Message { // Cacheline-size message
  struct {
    T *base;
    uint64_t len;
    uint64_t beg;
    uint64_t end;
    bool torswap;
  } arr;
  uint8_t pad[64]; // make sure it is cacheline-size message
  Message() {}
  Message(T *base, uint64_t len, uint64_t beg, uint64_t end, bool r=false) {
    arr.base = base;
    arr.len = len;
    arr.beg = beg;
    arr.end = end;
    arr.torswap = r;
  }
  Message(const Message &rhs) {
    arr.base = rhs.arr.base;
    arr.len = rhs.arr.len;
    arr.beg = rhs.arr.beg;
    arr.end = rhs.arr.end;
    arr.torswap = rhs.arr.torswap;
  }
} __attribute__((packed, aligned(64)));

/* Check if the array is ascending */
template <typename T>
void check(const T *arr, const uint64_t len) {
  for (uint64_t i = 1; len > i; ++i) {
    if (arr[i - 1] > arr[i]) {
      std::cout << "\033[91mERROR: arr[" << (i - 1) << "] = " << arr[i - 1] <<
        " > arr[" << i << "] = " << arr[i] << "\033[0m\n";
      break;
    }
  }
}

/* Dump an array */
template <typename T>
void dump(const T *arr, const uint64_t len) {
  std::cout << arr[0];
  for (uint64_t i = 1; len > i; ++i) {
    std::cout << ", " << arr[i];
  }
}

#ifdef DBG
template <typename T>
void print(const T *arr, const uint64_t len, const uint64_t beg,
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
template <typename T>
void swap(T *arr, const uint64_t len) {
  const uint64_t half = len >> 1;
  for (uint64_t i = 0; half > i; ++i) {
    const uint64_t pair = i + half;
    if (arr[i] > arr[pair]) {
      const T tmp = arr[i];
      arr[i] = arr[pair];
      arr[pair] = tmp;
    }
  }
}

/* Reverse an array */
template <typename T>
void reverse(T *arr, const uint64_t len) {
  const uint64_t half = len >> 1;
  for (uint64_t i = 0; half > i; ++i) {
    const uint64_t pair = len - i - 1;
    const T tmp = arr[i];
    arr[i] = arr[pair];
    arr[pair] = tmp;
  }
}

/* Swap values in the first half of a bitonic with corresponding values
 * in the reversed later half if the value from the first half is bigger */
template <typename T>
void rswap(T *arr, const uint64_t len) {
  const uint64_t half = len >> 1;
  for (uint64_t i = 0; half > i; ++i) {
    const uint64_t pair = len - i - 1;
    if (arr[i] > arr[pair]) {
      const T tmp = arr[i];
      arr[i] = arr[pair];
      arr[pair] = tmp;
    }
  }
}

/* Fill an array with a certain number */
template <typename T>
void fill(T *arr, const uint64_t len, const T val) {
#ifdef DBG
  std::cout << "fill(0x" << std::hex << (uint64_t)arr <<
    std::dec << ", " << len << ", " << val << ")\n";
#endif
  for (uint64_t i = 0; len > i; ++i) {
    arr[i] = val;
  }
}

/* Generate an unsorted array filled with random numbers */
template <typename T>
void gen(T *arr, const uint64_t len) {
#ifdef DBG
  std::cout << "gen(0x" << std::hex << (uint64_t)arr <<
    std::dec << ", " << len << ")\n";
#endif
  srand(2699);
  for (uint64_t i = 0; len > i; ++i) {
    arr[i] = rand();
  }
}

#endif
