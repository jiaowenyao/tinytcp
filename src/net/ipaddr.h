
#pragma once

#include <stdint.h>
#include <iostream>

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
    ipaddr_t(uint32_t addr) : type(IPADDR_V4), q_addr(addr) {}
    ipaddr_t(const char* str);

    const ipaddr_t operator=(const ipaddr_t& other) {
        type = other.type;
        q_addr = other.q_addr;
        return *this;
    }

    bool operator==(const ipaddr_t& other) {
        return q_addr == other.q_addr;
    }

    std::string to_string() const {
        return std::to_string(a_addr[0]) + "."
            +  std::to_string(a_addr[1]) + "."
            +  std::to_string(a_addr[2]) + "."
            +  std::to_string(a_addr[3]);
    }

};

std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr);

} // namespace tinytcp

