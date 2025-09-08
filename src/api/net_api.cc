#include "net_api.h"
#include "src/endiantool.h"
#include "src/net/net.h"
#include "src/net/sock.h"
#include "src/log.h"
#include <string.h>

namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

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



int socket(int family, int type, int protocol) {
    auto p = ProtocolStackMgr::get_instance();
    int sockfd = -2;
    p->exmsg_func_exec(std::bind(socket_create_req_in, family, type, protocol, std::ref(sockfd)));
    return sockfd;
}

ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (!buf || !len) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    if (dest_addr->sin_family != AF_INET || addrlen != sizeof(sockaddr_in)) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    auto p = ProtocolStackMgr::get_instance();
    int need_size = len;
    uint8_t* start = (uint8_t*)buf;
    while (need_size > 0) {
        int send_size = 0;
        p->exmsg_func_exec(std::bind(socket_sendto_req_in, sockfd, buf, need_size, flags,
                                     dest_addr, addrlen, std::ref(send_size)));

        need_size -= send_size;
        start += send_size;
    }

    return len - need_size;
}

} // namespace tinytcp

