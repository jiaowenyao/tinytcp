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

int close(int sock) {
    auto p = ProtocolStackMgr::get_instance();
    sock_req_t req;
    req.sockfd = sock;
    net_err_t err = p->exmsg_func_exec(std::bind(socket_close_req_in, &req));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "close error";
        return -1;
    }

    if (req.wait) {
        req.wait->wait_enter(req.wait_timeout);
    }
    return 0;
}

int setsockopt(int sockfd, int level, int optname,
                const void *optval, socklen_t optlen) {
    if (!optval || !optlen) {
        TINYTCP_LOG_ERROR(g_logger) << "setsockopt error param";
        return -1;
    }
    sock_req_t req;
    req.wait = nullptr;
    req.wait_timeout = -1;
    req.sockfd = sockfd;
    req.opt.level = level;
    req.opt.optname = optname;
    req.opt.optval = (char*)optval;
    req.opt.optlen = optlen;

    auto p = ProtocolStackMgr::get_instance();
    net_err_t err = p->exmsg_func_exec(std::bind(socket_setsocket_req_in, &req));
    if ((int8_t)err < 0) {
        return -1;
    }

    return 0;
}

ssize_t sendto(int sockfd, const void *buf, size_t size, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen) {
    if (!buf || !size) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    if (dest_addr->sin_family != AF_INET || addrlen != sizeof(sockaddr_in)) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    ssize_t send_size = 0;
    uint8_t* start = (uint8_t*)buf;
    auto p = ProtocolStackMgr::get_instance();
    while (size > 0) {
        sock_req_t req;
        req.wait = nullptr;
        req.sockfd = sockfd;
        req.data.buf = start;
        req.data.len = size;
        req.data.flags = flags;
        req.data.addr = (sockaddr*)dest_addr;
        req.data.addr_len = &addrlen;
        int send_size = 0;
        net_err_t err = p->exmsg_func_exec(std::bind(socket_sendto_req_in, &req));
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "write failed";
            return -1;
        }

        if (req.wait) {
            err = req.wait->wait_enter(req.wait_timeout);
            if ((int8_t)err < 0) {
                TINYTCP_LOG_ERROR(g_logger) << "write failed";
                return -1;
            }
        }

        size -= req.data.comp_len;
        send_size += req.data.comp_len;
        start += send_size;
    }

    return send_size;
}

ssize_t send(int sockfd, const void *buf, size_t size, int flags) {
    if (!buf || !size) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    ssize_t send_size = 0;
    uint8_t* start = (uint8_t*)buf;
    auto p = ProtocolStackMgr::get_instance();
    while (size > 0) {
        sock_req_t req;
        req.wait = nullptr;
        req.sockfd = sockfd;
        req.data.buf = start;
        req.data.len = size;
        req.data.flags = flags;
        req.data.addr = nullptr;
        req.data.addr_len = nullptr;
        int send_size = 0;
        net_err_t err = p->exmsg_func_exec(std::bind(socket_send_req_in, &req));
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "write failed";
            return -1;
        }

        if (req.wait) {
            err = req.wait->wait_enter(req.wait_timeout);
            if ((int8_t)err < 0) {
                TINYTCP_LOG_ERROR(g_logger) << "write failed";
                return -1;
            }
        }

        size -= req.data.comp_len;
        send_size += req.data.comp_len;
        start += send_size;
    }

    return send_size;
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                struct sockaddr *src_addr, socklen_t *addrlen) {
    if (!buf || !len || !src_addr) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    auto p = ProtocolStackMgr::get_instance();

    while (true) {
        sock_req_t req;
        req.wait = nullptr;
        req.sockfd = sockfd;
        req.data.buf = (uint8_t*)buf;
        req.data.len = len;
        req.data.comp_len = 0;
        req.data.addr = src_addr;
        req.data.addr_len = addrlen;
        net_err_t err = p->exmsg_func_exec(std::bind(socket_recvfrom_req_in, &req));
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "recv socket failed";
            return -1;
        }

        if (req.data.comp_len) {
            return req.data.comp_len;
        }

        err = req.wait->wait_enter(req.wait_timeout);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "recv failed";
            return -1;
        }
    }

    return 0;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    if (!buf || !len) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }

    auto p = ProtocolStackMgr::get_instance();

    while (true) {
        sock_req_t req;
        req.wait = nullptr;
        req.sockfd = sockfd;
        req.data.buf = (uint8_t*)buf;
        req.data.len = len;
        req.data.comp_len = 0;
        req.data.addr = nullptr;
        req.data.addr_len = nullptr;
        net_err_t err = p->exmsg_func_exec(std::bind(socket_recv_req_in, &req));
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "recv socket failed";
            return -1;
        }

        if (req.data.comp_len) {
            return req.data.comp_len;
        }

        err = req.wait->wait_enter(req.wait_timeout);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "recv failed";
            return -1;
        }
    }

    return 0;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!addr || addrlen != sizeof(sockaddr) || sockfd < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }
    if (addr->sin_family != AF_INET) {
        TINYTCP_LOG_ERROR(g_logger) << "family error";
        return -1;
    }
    sock_req_t req;
    req.sockfd = sockfd;
    req.conn.addr = (sockaddr*)addr;
    req.conn.addr_len = addrlen;
    auto p = ProtocolStackMgr::get_instance();
    net_err_t err = p->exmsg_func_exec(std::bind(socket_connect_req_in, &req));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "connect error";
        return -1;
    }

    if (req.wait && (int8_t)req.wait->wait_enter(req.wait_timeout) < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "connect failed";
        return -1;
    }

    return 0;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    if (!addr || addrlen != sizeof(sockaddr) || sockfd < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return -1;
    }
    if (addr->sin_family != AF_INET) {
        TINYTCP_LOG_ERROR(g_logger) << "family error";
        return -1;
    }
    sock_req_t req;
    req.sockfd = sockfd;
    req.bind.addr = (sockaddr*)addr;
    req.bind.addr_len = addrlen;
    auto p = ProtocolStackMgr::get_instance();
    net_err_t err = p->exmsg_func_exec(std::bind(socket_bind_req_in, &req));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "connect error";
        return -1;
    }

    return 0;
}


} // namespace tinytcp

