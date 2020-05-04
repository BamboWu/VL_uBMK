#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>

#include "threading.h"
#include "timing.h"

#define TARGET 256
uint64_t *rounds;
uint64_t *begs;
uint64_t *ends;
uint64_t *tab;
std::vector<std::string> messages;
volatile int wait_to_begin = 1;

void player(const int index) {

    setAffinity(index);
    uint64_t round = rounds[index];
    uint64_t count = ends[index] - begs[index];
    std::vector<uint64_t> seq(count);
    for (uint64_t i = 0; i < count; ++i) {
      seq[i] = begs[index] + i;
    }
    for (uint64_t i = count - 1; 0 < i; --i) {
      uint64_t idx = rand() % i;
      std::swap(seq[idx], seq[i]);
    }

    while (wait_to_begin);

    uint64_t beg = rdtsc();
    for (uint64_t r = 0; r < round; ++r) {
      for (uint64_t i = count - 1; 0 < i; --i) {
        uint64_t idx = rand() % i;
        std::swap(seq[idx], seq[i]);
        uint64_t pos = seq[i];
        __sync_add_and_fetch(&tab[pos], 1);
      }
      __sync_add_and_fetch(&tab[seq[0]], 1);
    }
    uint64_t end = rdtsc();
    int cpu = sched_getcpu();

    messages[index] = "Thread " + std::to_string(index) +
      " on CPU" + std::to_string(cpu) + " finishes in " +
      std::to_string(end - beg) + " ticks\n";

    return; // end of player function
}

int main(int argc, char *argv[]) {
  if (4 > argc) {
    std::cerr << argv[0] << " takes 3 arguments, " << argc << " given\n";
    std::cerr << "Usage: " << argv[0] << " <FSh|SpD> <#cores> <LLC in byte>\n";
    exit(-1);
  } else if ("FSh" != std::string(argv[1]) && "SpD" != std::string(argv[1])) {
    std::cerr << argv[0] << " takes either FSh or SpD as 1st argument\n";
    std::cerr << "Usage: " << argv[0] << " <FSh|SpD> <#cores> <LLC in byte>\n";
    exit(-1);
  }
  bool is_fsh = "FSh" == std::string(argv[1]);
  uint64_t ncores = strtoull(argv[2], NULL, 0);
  uint64_t nbytes = strtoull(argv[3], NULL, 0);

  // set global variables
  uint64_t count;
  rounds = new uint64_t[ncores];
  begs = new uint64_t[ncores];
  ends = new uint64_t[ncores];
  if (is_fsh) { // False Sharing (FSh): threads divides rounds
    for (uint64_t i = 0; i < (TARGET % ncores); ++i) {
      rounds[i] = (TARGET / ncores) + 1;
    }
    for (uint64_t i = TARGET % ncores; i < ncores; ++i) {
      rounds[i] = TARGET / ncores;
    }
    count = nbytes / sizeof(uint64_t) / (ncores + 1);
    for (uint64_t i = 0; i < ncores; ++i) {
      begs[i] = 0;
      ends[i] = count;
    }
  } else { // Space Divided (SpD): threads divides memory space
    for (uint64_t i = 0; i < ncores; ++i) {
      rounds[i] = TARGET;
    }
    count = nbytes / sizeof(uint64_t) / 2;
    uint64_t offset = 0;
    for (uint64_t i = 0; i < ncores; ++i) {
      begs[i] = offset;
      offset += count / ncores + (i < (count % ncores));
      ends[i] = offset;
    }
    assert(offset == count);
  }
  for (uint64_t i = 0; i < ncores; ++i) {
    std::cout << "Thread " << i << ": [" << begs[i] << ":" << ends[i] <<
      ")x" << rounds[i] << std::endl;
  }
  tab = static_cast<uint64_t*>(memalign(64, nbytes));
  for (uint64_t i = 0; i < count; ++i) {
    tab[i] = 0;
  }
  messages.resize(ncores);

  // go multi-threading
  std::vector<std::thread> threads;
  for (uint64_t i = 0; i < ncores; ++i) {
    threads.push_back(std::thread(player, i));
  }

  usleep(500);
  wait_to_begin = 0;

  for (uint64_t i = 0; i < ncores; ++i) {
    threads[i].join();
    std::cout << messages[i];
  }

  // check
  for (uint64_t i = 0; i < count; ++i) {
    if (TARGET != tab[i]) {
      std::cout << "\033[91mtab[" << i << "]=" << tab[i] << "\033[0m\n";
    }
  }

  // free allocations
  delete[] rounds;
  delete[] begs;
  delete[] ends;
  free(tab);

  return 0;
}
