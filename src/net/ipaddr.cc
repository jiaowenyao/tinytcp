#include "ipaddr.h"

namespace tinytcp {


std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr) {
    os  << std::to_string(ipaddr.a_addr[0])
        << '.' << std::to_string(ipaddr.a_addr[1])
        << '.' << std::to_string(ipaddr.a_addr[2])
        << '.' << std::to_string(ipaddr.a_addr[3]);
    return os;
}


}; // namespace tinytcp

