#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "zmq.h"

char fwd_socket_name[] = "ipc://echo_fwd";
char bwd_socket_name[] = "ipc://echo_bwd";

void server()
{
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

void client(char *msg)
{
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

    char __attribute__((aligned(64))) buf[64];
    for (rc = 0; 63 > rc && '\0' != msg[rc]; ++rc);
    msg[rc] = '\0';
    rc = zmq_send(sc, msg, rc + 1, 0);
    rc = zmq_recv(sb, buf, 64, 0);
    printf("%s\n", buf);

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
        client(argv[1]);
    } else {
        server();
    }
    return 0 ;
}
