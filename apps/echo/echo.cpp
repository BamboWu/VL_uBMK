#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <malloc.h>
#include "threading.h"
#include "timing.h"
#include "zmq.h"

#ifndef NOGEM5
#include "gem5/m5ops.h"
#endif

char fwd_socket_name[] = "ipc://echo_fwd";
char bwd_socket_name[] = "ipc://echo_bwd";

void server()
{
  setAffinity(0);
  void *ctx = zmq_ctx_new ();
  assert (ctx);

  void *sb = zmq_socket (ctx, ZMQ_PAIR);
  assert (sb);
  int rc = zmq_bind (sb, fwd_socket_name);
  assert (rc == 0);

  void *sc = zmq_socket (ctx, ZMQ_PAIR);
  assert (sc);
  rc = zmq_connect (sc, bwd_socket_name);
  assert (rc == 0);

  char __attribute__((aligned(64))) buf[64];
  while (true) {
    rc = zmq_recv(sb, buf, 64, 0);
    if (0 == strncmp ("quit", buf, 4)) {
      strcpy (buf, "Echo server quits. Goodbye!");
      rc = zmq_send (sc, buf, 28, 0);
      break;
    }
    int idx;
    for (idx = 0; idx < rc; ++idx) {
      if ('a' <= buf[idx] && buf[idx] <= 'z') {
        buf[idx] = buf[idx] - 'a' + 'A';
      }
    }
    rc = zmq_send (sc, buf, rc, 0);
  }

  rc = zmq_close (sc);
  assert (rc == 0);

  rc = zmq_close (sb);
  assert (rc == 0);

  rc = zmq_ctx_term (ctx);
  assert (rc == 0);
}

void client(int msgc, char *msgv[])
{
  setAffinity(1);
  void *ctx = zmq_ctx_new ();
  assert (ctx);

  void *sb = zmq_socket (ctx, ZMQ_PAIR);
  assert (sb);
  int rc = zmq_bind (sb, bwd_socket_name);
  assert (rc == 0);

  void *sc = zmq_socket (ctx, ZMQ_PAIR);
  assert (sc);
  rc = zmq_connect (sc, fwd_socket_name);
  assert (rc == 0);

  char *buf = (char*) memalign(64, msgc << 6);
  const uint64_t beg = rdtsc();
#ifndef NOGEM5
  m5_reset_stats(0, 0);
#endif
  for (int idx = 0; msgc > idx; ++idx) {
    for (rc = 0; 63 > rc && '\0' != msgv[idx][rc]; ++rc);
    msgv[idx][rc] = '\0';
    rc = zmq_send(sc, msgv[idx], rc + 1, 0);
    rc = zmq_recv(sb, &buf[idx << 6], 64, 0);
  }
#ifndef NOGEM5
  m5_dump_stats(0, 0);
#endif
  const uint64_t end = rdtsc();
  for (int idx = 0; msgc > idx; ++idx) {
    printf("%s\n", &buf[idx << 6]);
  }
  printf("ticks (RTT): %" PRIu64 "\n", end - beg);
  free(buf);

  rc = zmq_close (sc);
  assert (rc == 0);

  rc = zmq_close (sb);
  assert (rc == 0);

  rc = zmq_ctx_term (ctx);
  assert (rc == 0);
}

int main (int argc, char *argv[])
{
  if (1 < argc) {
    client(argc - 1, &argv[1]);
  } else {
    server();
  }
  return 0 ;
}
