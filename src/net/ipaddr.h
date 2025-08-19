
#pragma once

#include <stdint.h>

namespace tinytcp {

#define IPV4_ADDR_SIZE  4

struct ipaddr_t {

    enum {
        IPADDR_V4,
    } type;

    union {
        uint32_t q_addr;
        uint8_t  a_addr[IPV4_ADDR_SIZE];
    };

    ipaddr_t() : type(IPADDR_V4), q_addr(0) {}
};


} // namespace tinytcp

