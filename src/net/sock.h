#pragma once

#include "net_err.h"
#include "ipaddr.h"
#include <memory>

namespace tinytcp {

#define IPV4_STR_SIZE 16
#undef INADDR_ANY
#define INADDR_ANY    (uint32_t)0x00000000;

#undef AF_INET
#define AF_INET    2

#undef SOCK_RAW
#define SOCK_RAW   0

#undef IPPROTO_ICMP
#define IPPROTO_ICMP  1


using socklen_t = int;

struct sockaddr {
    // uint8_t sin_len;
    // uint8_t sin_family;
    uint16_t sin_family;
    uint8_t sa_data[14];
};

class Sock;

struct socket_t {
    using ptr = std::shared_ptr<socket_t>;

    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED,
    } state;

    Sock* sock;
};

class Sock {
public:
    Sock(int family, int protocol);
    virtual ~Sock() = default;

    virtual net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) = 0;
    virtual net_err_t recvfrom(const void* buf, size_t len, int flags,
                             const struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) = 0;
    virtual net_err_t setopt(int level, int optname,
                             const char* optval, int optlen) = 0;
    virtual void destory() {};

protected:
    uint16_t m_local_port = 0;;
    ipaddr_t m_local_ip;
    uint16_t m_remote_port = 0;
    ipaddr_t m_remote_ip;

    int m_family;
    int m_protocol;
    net_err_t m_err = net_err_t::NET_ERR_OK;
    int m_recv_timeout = 0;
    int m_send_timeout = 0;

};

net_err_t socket_init();
net_err_t socket_create_req_in(int family, int type, int protocol, int& sockfd);
net_err_t socket_sendto_req_in(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen, int& send_size);

} // namespace tinytcp

