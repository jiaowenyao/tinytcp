#include "ipaddr.h"

namespace tinytcp {


ipaddr_t::ipaddr_t(const char* str) {
    type = IPADDR_V4;
    if (!str) {
        return;
    }

    q_addr = 0;
    uint8_t* p = a_addr;
    uint8_t sub_addr = 0;
    char c;
    while ((c = *str++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            sub_addr = sub_addr * 10 + (c - '0');
        }
        else if (c == '.') {
            *p++ = sub_addr;
            sub_addr = 0;
        }
        else {
            return;
        }
    }
    *p = sub_addr;
}

bool ipaddr_is_local_broadcast(const ipaddr_t& ipaddr) {
    return ipaddr.q_addr == 0xFFFFFFFF;
}

bool ipaddr_is_direct_broadcast(const ipaddr_t& ipaddr, const ipaddr_t& netmask) {
    uint32_t host_ip = ipaddr.q_addr & (~netmask.q_addr);
    return host_ip == (0xFFFFFFFF & (~netmask.q_addr));
}

bool operator==(const ipaddr_t& a, const ipaddr_t& b) {
    return a.q_addr == b.q_addr;
}


std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr) {
    os  << std::to_string(ipaddr.a_addr[0])
        << '.' << std::to_string(ipaddr.a_addr[1])
        << '.' << std::to_string(ipaddr.a_addr[2])
        << '.' << std::to_string(ipaddr.a_addr[3]);
    return os;
}

ipaddr_t ipaddr_get_net(const ipaddr_t& ipaddr, const ipaddr_t& netmask) {
    ipaddr_t netid;
    netid.q_addr = ipaddr.q_addr & netmask.q_addr;
    return netid;
}

bool ipaddr_is_match(const ipaddr_t& dest_ip, const ipaddr_t& netif_ip, const ipaddr_t& netif_netmask) {
    if (ipaddr_is_local_broadcast(dest_ip)) {
        return true;
    }

    ipaddr_t dest_netid = ipaddr_get_net(dest_ip, netif_netmask);
    ipaddr_t src_netid = ipaddr_get_net(netif_ip, netif_netmask);
    if (ipaddr_is_direct_broadcast(dest_ip, netif_netmask) && (dest_netid == src_netid)) {
        return true;
    }

    return dest_ip == netif_ip;
}

}; // namespace tinytcp

