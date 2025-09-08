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
    sock->sock->reset_recv_wait();
    sock->sock->reset_send_wait();
    sock->sock->reset_conn_wait();
}

void sock_wait_t::wait_add(uint32_t timeout, sock_req_t* req) {
    req->wait = this;
    req->wait_timeout = timeout;
    ++waiting_cnt;
}

void sock_wait_t::wait_leave(net_err_t _err) {
    if (waiting_cnt > 0) {
        --waiting_cnt;
        err = _err;
        sem.notify();
    }
}

net_err_t sock_wait_t::wait_enter(int timeout) {
    if (timeout < 0) {
        sem.wait();
    }
    else if (sem.wait_timeout(timeout) < 0) {
        return net_err_t::NET_ERR_TIMEOUT;
    }
    return err;
}

void Sock::reset_recv_wait(sock_wait_t* wait) noexcept {
    if (m_recv_wait != nullptr) {
        delete m_recv_wait;
    }
    m_recv_wait = wait;
}

void Sock::reset_send_wait(sock_wait_t* wait) noexcept {
    if (m_send_wait != nullptr) {
        delete m_send_wait;
    }
    m_send_wait = wait;
}

void Sock::reset_conn_wait(sock_wait_t* wait) noexcept {
    if (m_conn_wait != nullptr) {
        delete m_conn_wait;
    }
    m_conn_wait = wait;
}

void Sock::wakeup(int type, net_err_t err) {
    if (type & SOCK_WAIT_CONN) {
        m_conn_wait->wait_leave(err);
    }
    if (type & SOCK_WAIT_WRITE) {
        m_send_wait->wait_leave(err);
    }
    if (type & SOCK_WAIT_READ) {
        m_recv_wait->wait_leave(err);
    }
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

net_err_t socket_setsocket_req_in(sock_req_t* req) {

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

net_err_t socket_sendto_req_in(sock_req_t* req) {

    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }

    Sock* sock = s->sock;
    net_err_t err = sock->sendto(req->data.buf, req->data.len, req->data.flags,
                                 req->data.addr, *req->data.addr_len, &req->data.comp_len);
    if (err == net_err_t::NET_ERR_NEED_WAIT) {
        auto send_wait = sock->get_send_wait();
        if (send_wait) {
            send_wait->wait_add(sock->get_send_timeout(), req);
        }
    }
    return err;
}


net_err_t socket_recvfrom_req_in(sock_req_t* req) {

    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }

    Sock* sock = s->sock;
    net_err_t err = sock->recvfrom(req->data.buf, req->data.len, req->data.flags,
                                   req->data.addr, *req->data.addr_len, &req->data.comp_len);
    if (err == net_err_t::NET_ERR_NEED_WAIT) {
        auto recv_wait = sock->get_recv_wait();
        if (recv_wait) {
            recv_wait->wait_add(sock->get_recv_timeout(), req);
        }
    }

    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp

