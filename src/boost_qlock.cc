#include "boost_qlock.h"
#include <iostream>
#include <boost/lockfree/queue.hpp>

void *get_boost_qlock() {
  boost::lockfree::queue<int> *qlock = new boost::lockfree::queue<int>(1);
  qlock->push(0);
  return (void*) qlock;
}

void boost_qlock_acquire(void *lock) {
  boost::lockfree::queue<int> *qlock = (boost::lockfree::queue<int>*) lock;
  int tmp = 0;
  while(!qlock->pop(tmp)) { }
}

void boost_qlock_release(void *lock) {
  boost::lockfree::queue<int> *qlock = (boost::lockfree::queue<int>*) lock;
  qlock->push(0);
}
