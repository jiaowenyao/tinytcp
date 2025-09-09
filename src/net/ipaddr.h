
#pragma once

#include <stdint.h>
#include <iostream>

namespace tinytcp {

#define IPV4_ADDR_SIZE           4
#define NET_VERSION_IPV4         4
#define NET_IP_DEFAULT_TTL       64

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

    bool is_empty() const {
        return q_addr == 0;
    }

};

bool is_local_broadcast(const ipaddr_t& ipaddr);
bool is_direct_broadcast(const ipaddr_t& ipaddr, const ipaddr_t& netmask);
bool operator==(const ipaddr_t& a, const ipaddr_t& b);
ipaddr_t ipaddr_get_net(const ipaddr_t& ipaddr, const ipaddr_t& netmask);

std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr);

bool ipaddr_is_match(const ipaddr_t& dest_ip, const ipaddr_t& netif_ip, const ipaddr_t& netif_netmask);

} // namespace tinytcp

