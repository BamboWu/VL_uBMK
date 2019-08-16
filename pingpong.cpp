#include <iostream>
#include <pthread.h>
#include <sys/sysinfo.h>
#include <thread>
#include <chrono>
#include <array>
#include <time.h>
#include <boost/lockfree/queue.hpp>
#include <vl.h>
#ifdef GEM5
#include "gem5/m5ops.h"
#endif

#ifdef PAUSE
#define PAUSE_ASM __asm__ volatile("pause" : : :)
#else
#define PAUSE_ASM __asm__ volatile("nop" : : :)
#endif

#define CAPACITY 4096

typedef std::array<double, 7> ball_t;
boost::lockfree::queue<ball_t> left_lockfree {CAPACITY / sizeof(ball_t)};
boost::lockfree::queue<ball_t> right_lockfree {CAPACITY / sizeof(ball_t)};

int left_vl_fd;
int right_vl_fd;
vlendpt_t left_prod_vl, left_cons_vl, right_prod_vl, right_cons_vl;

struct playerArgs {
    bool *pwait; // all threads controlled by main for timing/statistic
    char mech;
    uint64_t round;
    bool left; // left player initiate the ping-pong
};

void
player(playerArgs *pargs) {

    const uint64_t round = pargs->round;
    bool left = pargs->left;
    bool my_serve = left;
    ball_t ball;
    uint64_t *pdouble;
    bool valid;
    double sum;

    // set affinity
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(left?0:1, &cpuset);
    if (0 != pthread_setaffinity_np(pthread_self(),
                                    sizeof(cpu_set_t), &cpuset)) {
        std::cout << "\033[91mFailed to set affinity for " <<
          pthread_self() << "\033[0m\n";
    }

    if ('f' == pargs->mech) {
      boost::lockfree::queue<ball_t> *psend =
        left ? &left_lockfree : &right_lockfree;
      boost::lockfree::queue<ball_t> *precv =
        left ? &right_lockfree : &left_lockfree;
      while (*(pargs->pwait)) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
      for (uint64_t r = 1; round >= r; ++r) {
        std::cout << (left ? "L" : "R") << " @ CPU " << sched_getcpu() << "\n";

        if (my_serve) {
          for (uint64_t i = 0; 7 > i; ++i) {
            ball[i] = 0.0714 * r / round + i / 42.0;
          }
          psend->push(ball);
          my_serve = false;
        } else {
          while(!(precv->pop(ball))) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(1));
          }
          sum = 0;
          for (uint64_t i = 0; 7 > i; ++i) {
            sum += ball[i];
            //std::cout << "ball[" << i << "]=" << ball[i] << std::endl;
          }
          std::cout << "\033[92m" << (left ? "L " : "R ") << sum <<
            "\033[0m\n";
          my_serve = true;
        }
      }
    } else if ('V' == pargs->mech) {
      vlendpt_t *psend_vl = left ? &left_prod_vl : &right_prod_vl;
      vlendpt_t *precv_vl = left ? &right_cons_vl : &left_cons_vl;
      while (*(pargs->pwait)) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
      for (uint64_t r = 1; round >= r; ++r) {
        std::cout << (left ? "L" : "R") << " @ CPU " << sched_getcpu() << "\n";

        if (my_serve) {
          for (uint64_t i = 0; 7 > i; ++i) {
            ball[i] = 0.0714 * r / round + i / 42.0;
            pdouble = (uint64_t*)&ball[i];
            double_vl_push_strong(psend_vl, *pdouble);
          }
          my_serve = false;
        } else {
          for (uint64_t i = 0; 7 > i;) {
            pdouble = (uint64_t*)&ball[i];
            double_vl_pop_non(precv_vl, pdouble, &valid);
            if (valid) {
              i++;
            } else {
              std::this_thread::sleep_for(std::chrono::nanoseconds(1));
            }
          }
          sum = 0;
          for (uint64_t i = 0; 7 > i; ++i) {
            sum += ball[i];
            //std::cout << "ball[" << i << "]=" << ball[i] << std::endl;
          }
          std::cout << "\033[92m" << (left ? "L " : "R ") << sum <<
            "\033[0m\n";
          my_serve = true;
        }
      }
    }
}

int main(int argc, char *argv[]) {

    char mech = 'f'; // 'f' for lockfree, 'V' for Virtual Link
    uint64_t round = 10;

    if (1 < argc) {
        mech = argv[1][0];
    }
    if (2 < argc) {
        round = atoll(argv[2]);
    }

    std::cout << argv[0] << " " << mech << " " << round << std::endl;

    if ('f' == mech) { // do not mkvl() for freelock, since no rmvl() following
    } else {
        left_vl_fd = mkvl();
        if (0 > left_vl_fd) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return left_vl_fd;
        }
        open_double_vl_as_producer(left_vl_fd, &left_prod_vl, 1);
        open_double_vl_as_consumer(left_vl_fd, &left_cons_vl, 1);
        right_vl_fd = mkvl();
        if (0 > right_vl_fd) {
            std::cerr << "mkvl() return invalid file descriptor\n";
            return right_vl_fd;
        }
        open_double_vl_as_producer(right_vl_fd, &right_prod_vl, 1);
        open_double_vl_as_consumer(right_vl_fd, &right_cons_vl, 1);
    }

    std::cout << "There are " << get_nprocs() << " CPUs available.\n";

    bool wait = true;
    playerArgs args[2];

    args[0].pwait = &wait;
    args[0].mech = mech;
    args[0].round = round;
    args[0].left = true;
    args[1].pwait = &wait;
    args[1].mech = mech;
    args[1].round = round;
    args[1].left = false;

    std::thread playerl(player, &args[0]);
    std::thread playerr(player, &args[1]);

    timespec beg, end;

    if (clock_gettime(CLOCK_REALTIME, &beg)) {
        std::cerr << "fail to get begin real time\n";
    }

#ifdef GEM5
    m5_reset_stats(0, 0);
#endif
    wait = false;

    playerl.join();
    playerr.join();
#ifdef GEM5
    m5_dump_reset_stats(0, 0);
#endif

    if (clock_gettime(CLOCK_REALTIME, &end)) {
        std::cerr << "fail to get end real time\n";
    }

    long seconds = end.tv_sec - beg.tv_sec;
    long nanosec = end.tv_nsec - beg.tv_nsec;

    if (0 > nanosec) { // underflow
        nanosec += 1000000000;
        --seconds;
    }
    std::cout << seconds << "s " << nanosec << "ns elapsed\n";

    if ('f' == mech) {
    } else { // clean up
        close_double_vl_as_producer(left_prod_vl);
        close_double_vl_as_consumer(left_cons_vl);
        close_double_vl_as_producer(right_prod_vl);
        close_double_vl_as_consumer(right_cons_vl);
    }

    return 0;
}
