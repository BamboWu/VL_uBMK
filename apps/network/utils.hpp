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
#define POOL_SIZE 16 // #2MB packets
#endif

#define HEADER_SIZE 44

struct Packet {
    // IPv4 header
    uint8_t version;
    uint8_t service;
    uint16_t len;
    uint16_t id;
    uint16_t flagsIP;
    uint8_t TTL;
    uint8_t protocol;
    uint16_t checksumIP;
    uint64_t srcIP;
    uint64_t dstIP;
    // TCP header
    uint16_t srcPort;
    uint16_t dstPort;
    uint64_t seqNum;
    uint64_t ackNum;
    uint16_t flagsTCP;
    uint16_t winSize;
    uint16_t checksumTCP;
    uint16_t urgentPtr;
    // payload
    void * payload;
};

#endif // end of ifndef _NETWORK_UTILS_HPP__
