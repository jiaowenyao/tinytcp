#pragma once
#include <inttypes.h>
#include "src/net/icmp.h"
#include "src/net/ip.h"

#define PING_BUFFER_SIZE        4096
#define PING_DEFAULT_ID         0x300

#pragma pack(1)
struct icmpv4_hdr_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};
#pragma pack()

struct PingOptions {
    std::string target;
    uint32_t count = 4;        // 默认ping 4次
    uint32_t packet_size = 32; // 默认包大小32字节
    uint32_t timeout = 3000;   // 默认超时3秒
    uint32_t interval = 1000;  // 循环时间
};

struct echo_req_t {
    icmpv4_hdr_t echo_hdr;
    char buf[PING_BUFFER_SIZE];
};

struct echo_reply_t {
    tinytcp::ipv4_hdr_t ip_hdr;
    icmpv4_hdr_t echo_hdr;
    char buf[PING_BUFFER_SIZE];
};

struct ping_t {
    echo_req_t req;
    echo_reply_t reply;
};

void ping_run(const ping_t& ping, const PingOptions& options);

