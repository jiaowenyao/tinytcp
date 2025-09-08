#include "net_api.h"
#include "src/endiantool.h"
#include <string.h>

namespace tinytcp {

#define IPV4_STR_SIZE 16
#undef INADDR_ANY
#define INADDR_ANY    (uint32_t)0x00000000;
#undef AF_INET
#define AF_INET    2

uint32_t htonl(uint32_t hostlong)  { return host_to_net(hostlong);  }
uint16_t htons(uint16_t hostshort) { return host_to_net(hostshort); }
uint32_t ntohl(uint32_t netlong)   { return net_to_host(netlong);   }
uint16_t ntohs(uint16_t netshort)  { return net_to_host(netshort);  }

char* inet_ntoa(struct in_addr in) {
    static char buf[IPV4_STR_SIZE];
    sprintf(buf, "%d.%d.%d.%d", in.addr0, in.addr1, in.addr2, in.addr3);
    return buf;
}

uint32_t inet_addr(const char* str) {
    if (!str) {
        return INADDR_ANY;
    }
    ipaddr_t ipaddr(str);
    return ipaddr.q_addr;
}
int inet_pton(int family, const char *src, void *dst) {
    if ((family != AF_INET) || !src || !dst) {
        return -1;
    }
    ipaddr_t ipaddr(src);
    in_addr* addr = (in_addr*)(dst);
    addr->s_addr = ipaddr.q_addr;
    return 1;
}

const char *inet_ntop(int af, const void *src, char *dst, size_t size) {
    if ((af != AF_INET) || !src || !dst) {
        return nullptr;
    }

    in_addr* addr = (in_addr*)src;
    char buf[IPV4_STR_SIZE];
    sprintf(buf, "%d.%d.%d.%d", addr->addr0, addr->addr1, addr->addr2, addr->addr3);
    strncpy(dst, buf, size - 1);
    dst[size - 1] = '\0';
    return dst;
}



} // namespace tinytcp

