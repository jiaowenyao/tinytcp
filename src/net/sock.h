#pragma once

#include "net_err.h"
#include "ipaddr.h"
#include "pktbuf.h"
#include "src/mutex.h"
#include <memory>

namespace tinytcp {

#define IPV4_STR_SIZE 16
#undef INADDR_ANY
#define INADDR_ANY    (uint32_t)0x00000000;

#undef AF_INET
#define AF_INET    2

#undef SOCK_RAW
#define SOCK_RAW   0
#undef SOCK_DGRAM
#define SOCK_DGRAM 1

#undef IPPROTO_ICMP
#define IPPROTO_ICMP  1
#undef IPPROTO_UDP
#define IPPROTO_UDP   17

#undef SOL_SOCKET
#define SOL_SOCKET     0
#undef SO_RCVTIMEO
#define SO_RCVTIMEO    1
#undef SO_SNDTIMEO
#define SO_SNDTIMEO    2


using socklen_t = int;

struct sockaddr {
    // uint8_t sin_len;
    // uint8_t sin_family;
    uint16_t sin_family;
    uint8_t sa_data[14];
};

struct timeval {
    int tv_sec;
    int tv_usec;
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

#define SOCK_WAIT_READ      (1 << 0)
#define SOCK_WAIT_WRITE     (1 << 1)
#define SOCK_WAIT_CONN      (1 << 2)
#define SOCK_WAIT_ALL       (SOCK_WAIT_READ | SOCK_WAIT_WRITE | SOCK_WAIT_CONN)

struct sock_req_t;
struct sock_wait_t {
    Semaphore sem;
    net_err_t err;
    int waiting_cnt;
    sock_wait_t() : sem(0), err(net_err_t::NET_ERR_OK), waiting_cnt(0) {}
    void wait_add(uint32_t timeout, sock_req_t* req);
    void wait_leave(net_err_t err);
    net_err_t wait_enter(int timeout);
};

struct sock_data_t {
    uint8_t* buf;
    size_t len;
    int flags;
    sockaddr* addr;
    socklen_t* addr_len;
    ssize_t comp_len;
};

struct sock_opt_t {
    int level;
    int optname;
    const char* optval;
    int optlen;
};

struct sock_req_t {
    sock_wait_t* wait = nullptr;
    int wait_timeout = -1; // -1阻塞，0不阻塞，大于0为定时
    int sockfd;
    union {
        sock_data_t data;
        sock_opt_t  opt;
    };
};


class Sock {
public:
    Sock(int family, int protocol);
    virtual ~Sock() = default;

    virtual void init() {}

    virtual net_err_t sendto(const void* buf, size_t len, int flags,
                             const struct sockaddr* dest, socklen_t dest_len,
                             ssize_t* result_len) = 0;
    virtual net_err_t recvfrom(const void* buf, size_t len, int flags,
                             struct sockaddr* src, socklen_t src_len,
                             ssize_t* result_len) = 0;
    virtual net_err_t setopt(int level, int optname,
                             const char* optval, int optlen);
    virtual net_err_t close();

    uint16_t get_local_port() const noexcept { return m_local_port; }
    uint16_t get_remote_port() const noexcept { return m_remote_port; }
    void set_local_port(uint16_t v) noexcept { m_local_port = v; }
    void set_remote_port(uint16_t v) noexcept { m_remote_port = v; }
    const ipaddr_t& get_local_ip() const noexcept { return m_local_ip; }
    const ipaddr_t& get_remote_ip() const noexcept { return m_remote_ip; }
    void set_local_ip(const ipaddr_t& ip) noexcept { m_local_ip = ip; }
    void set_remote_ip(const ipaddr_t& ip) noexcept { m_remote_ip = ip; }

    int get_recv_timeout() const noexcept { return m_recv_timeout; }
    int get_send_timeout() const noexcept { return m_send_timeout; }

    sock_wait_t* get_recv_wait() const noexcept { return m_recv_wait; }
    sock_wait_t* get_send_wait() const noexcept { return m_send_wait; }
    sock_wait_t* get_conn_wait() const noexcept { return m_conn_wait; }

    void reset_recv_wait(sock_wait_t* wait = nullptr) noexcept;
    void reset_send_wait(sock_wait_t* wait = nullptr) noexcept;
    void reset_conn_wait(sock_wait_t* wait = nullptr) noexcept;

    void wakeup(int type, net_err_t err);
    bool push_buf(PktBuffer::ptr& buf);
    PktBuffer::ptr pop_buf(int timeout = 0);

public:
    static Sock* find_sock(const ipaddr_t& src, const ipaddr_t& dest, int protocol);

protected:
    uint16_t m_local_port = 0;;
    ipaddr_t m_local_ip;
    uint16_t m_remote_port = 0;
    ipaddr_t m_remote_ip;

    int m_family;
    int m_protocol;
    net_err_t m_err = net_err_t::NET_ERR_OK;
    int m_recv_timeout = -1;
    int m_send_timeout = -1;

    sock_wait_t* m_recv_wait = nullptr;
    sock_wait_t* m_send_wait = nullptr;
    sock_wait_t* m_conn_wait = nullptr;

    LockFreeRingQueue<PktBuffer::ptr> m_buf_queue;
};

net_err_t socket_init();
net_err_t socket_close_req_in(sock_req_t* req);
net_err_t socket_setsocket_req_in(sock_req_t* req);
net_err_t socket_create_req_in(int family, int type, int protocol, int& sockfd);
net_err_t socket_sendto_req_in(sock_req_t* req);
net_err_t socket_recvfrom_req_in(sock_req_t* req);
} // namespace tinytcp

