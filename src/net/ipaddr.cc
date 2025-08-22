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

std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr) {
    os  << std::to_string(ipaddr.a_addr[0])
        << '.' << std::to_string(ipaddr.a_addr[1])
        << '.' << std::to_string(ipaddr.a_addr[2])
        << '.' << std::to_string(ipaddr.a_addr[3]);
    return os;
}


}; // namespace tinytcp

