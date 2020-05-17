#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include "check.h"

#ifndef NOPAPI
#include "papi.h" /* This needs to be included every time you use PAPI */

static bool initialized = false;
static bool started = false;
static int events = PAPI_NULL;
static int retval;

/*
 * Initialize PAPI libaray.
 */
static void initializePAPI() {
  retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    printf("Failed with %d at PAPI_library_init()", retval);
    exit(1);
  }
  initialized = true;
}

/*
 * Start PAPI to profile number of cycles.
 */
void cycleBeg() {

  if (started) {
    printf("Have already started PAPI!\n");
    return;
  }

  if (!initialized) { initializePAPI(); }

  retval = PAPI_create_eventset(&events);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_TOT_CYC);
  if (retval < PAPI_OK) { errorReturn(retval); }

  retval = PAPI_start(events);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = true;
}

/*
 * Stop PAPI and read the number of cycles.
 */
long long cycleEnd() {

  long long cntval;
  retval = PAPI_stop(events, &cntval);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = false;
  events = PAPI_NULL;

  return cntval;
}

/*
 * Start PAPI to profile number of L1D accesses, reads, writes, misses.
 */
void l1dBeg() {

  if (started) {
    printf("Have already started PAPI!\n");
    return;
  }

  if (!initialized) { initializePAPI(); }

  retval = PAPI_create_eventset(&events);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L1_DCA);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L1_DCR);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L1_DCW);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L1_DCM);
  if (retval < PAPI_OK) { errorReturn(retval); }

  retval = PAPI_start(events);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = true;
}

/*
 * Stop PAPI and read the number of L1D accesses, reads, writes, misses.
 */
void l1dEnd(long long *cntvals) {

  retval = PAPI_stop(events, cntvals);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = false;
  events = PAPI_NULL;

  return;
}

/*
 * Start PAPI to profile number of L2D reads, writes, misses.
 */
void l2dBeg() {

  if (started) {
    printf("Have already started PAPI!\n");
    return;
  }

  if (!initialized) { initializePAPI(); }

  retval = PAPI_create_eventset(&events);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCR);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCW);
  if (retval < PAPI_OK) { errorReturn(retval); }
  retval = PAPI_add_event(events, PAPI_L2_DCM);
  if (retval < PAPI_OK) { errorReturn(retval); }

  retval = PAPI_start(events);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = true;
}

/*
 * Stop PAPI and read the number of L2D accesses, reads, writes, misses.
 */
void l2dEnd(long long *cntvals) {

  retval = PAPI_stop(events, cntvals);
  if (retval < PAPI_OK) { errorReturn(retval); }

  started = false;
  events = PAPI_NULL;

  return;
}

#else

void cycleBeg() { printf("NOPAPI!\n"); }
long long cycleEnd() { printf("NOPAPI!\n"); return 0; }
void l1dBeg() { printf("NOPAPI!\n"); }
void l1dEnd(long long *cntvals) { printf("NOPAPI!\n"); }
void l2dBeg() { printf("NOPAPI!\n"); }
void l2dEnd(long long *cntvals) { printf("NOPAPI!\n"); }

#endif /* defined(NOPAPI) */
