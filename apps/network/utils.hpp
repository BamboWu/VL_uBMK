#ifndef _NETWORK_UTILS_HPP__
#define _NETWORK_UTILS_HPP__

#include <stdint.h>

#ifndef NUM_STAGE1
#define NUM_STAGE1 1
#endif

#ifndef NUM_STAGE2
#define NUM_STAGE2 1
#endif

#ifndef POOL_SIZE
#define POOL_SIZE 56 // #2KB packets
#endif

#ifndef BULK_SIZE
#define BULK_SIZE 8
#endif

#ifndef MISTAKE_GATHER_RETRY
#define MISTAKE_GATHER_RETRY 64
#endif

#define HEADER_SIZE 192

struct IPv4Header {
    uint8_t version;
    uint8_t service;
    uint16_t len;
    uint16_t id;
    uint16_t flagsIP;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t checksumIP;
    uint32_t srcIP;
    uint32_t dstIP;
} __attribute__((packed));

struct TCPHeader {
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint16_t flagsTCP;
    uint16_t winSize;
    uint16_t checksumTCP;
    uint16_t urgentPtr;
} __attribute__((packed));

struct Packet {
    // IPv4 header
    union {
        IPv4Header data;
        uint8_t pad[64];
    } __attribute__((packed)) ipheader;
    // TCP header
    union {
        TCPHeader data;
        uint8_t pad[64];
    } __attribute__((packed)) tcpheader;
    // payload
    void * payload;
} __attribute__((packed, aligned(64)));

#define STATIC_ASSERT(COND,MSG) typedef char static_assert_##MSG[(COND)?1:-1]

STATIC_ASSERT(HEADER_SIZE >= sizeof(Packet), PacketSize);

#endif // end of ifndef _NETWORK_UTILS_HPP__
