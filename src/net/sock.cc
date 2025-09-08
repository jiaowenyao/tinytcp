#include "sock.h"
#include "src/config.h"
#include "src/log.h"
#include "src/net/raw.h"

namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");


static tinytcp::ConfigVar<uint32_t>::ptr g_socket_max_size =
    tinytcp::Config::look_up("socket.max_size", 1024U, "最多能分配多少个socket");

static socket_t* socket_tbl;


static int get_index(socket_t* sock) {
    return (int)(sock - socket_tbl);
}

static socket_t* get_socket(int idx) {
    if (idx < 0 || idx >= g_socket_max_size->value()) {
        return nullptr;
    }
    return socket_tbl + idx;
}

static socket_t* socket_alloc() noexcept {
    for (int i = 0; i < g_socket_max_size->value(); ++i) {
        socket_t* curr = socket_tbl + i;
        if (curr->state == socket_t::SOCKET_STATE_FREE) {
            curr->state = socket_t::SOCKET_STATE_USED;
            return curr;
        }
    }
    return nullptr;
}

static void socket_free(socket_t* sock) {
    sock->state = socket_t::SOCKET_STATE_FREE;
}

Sock::Sock(int family, int protocol)
    : m_family(family)
    , m_protocol(protocol) {
    m_local_ip.q_addr = 0;
    m_remote_ip.q_addr = 0;
}

net_err_t socket_init() {
    socket_tbl = new socket_t[g_socket_max_size->value()];

    return net_err_t::NET_ERR_OK;
}

net_err_t socket_create_req_in(int family, int type, int protocol, int& sockfd) {
    socket_t* sock = socket_alloc();
    sockfd = -1;
    if (sock == nullptr) {
        TINYTCP_LOG_ERROR(g_logger) << "no socket";
        return net_err_t::NET_ERR_MEM;
    }
    switch (type) {
        case SOCK_RAW: {
            sock->sock = new RAWSock(family, protocol);
            break;
        }
    }
    sockfd = get_index(sock);
    return net_err_t::NET_ERR_OK;
}

net_err_t socket_sendto_req_in(int sockfd, const void *buf, size_t len, int flags,
                const struct sockaddr *dest_addr, socklen_t addrlen, int& send_size) {

    socket_t* s = get_socket(sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }

    Sock* sock = s->sock;
    net_err_t err = sock->sendto(buf, len, flags, dest_addr, addrlen, (ssize_t*)&send_size);

    return err;
}

} // namespace tinytcp

