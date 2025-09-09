#pragma once

#include <inttypes.h>
#include <stdlib.h>
#include "src/net/ip.h"
#include "src/net/sock.h"

namespace tinytcp {

struct in_addr {
    union {
        struct {
            uint8_t addr0;
            uint8_t addr1;
            uint8_t addr2;
            uint8_t addr3;
        };
        uint8_t addr_array[IPV4_ADDR_SIZE];
        #undef s_addr
        uint32_t s_addr;
    };
};

struct sockaddr_in {
    // uint8_t sin_len;
    // uint8_t sin_family;
    uint16_t sin_family;
    uint16_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);

char* inet_ntoa(struct in_addr in);
uint32_t inet_addr(const char* str);
int inet_pton(int family, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, size_t size);


int socket(int family, int type, int protocol);
int close(int sock);
int setsockopt(int sockfd, int level, int optname,
                const void *optval, socklen_t optlen);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                struct sockaddr *src_addr, socklen_t *addrlen);



} // namespace tinytcp



