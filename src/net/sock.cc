#include "sock.h"
#include "src/config.h"
#include "src/log.h"
#include "src/net/raw.h"
#include "src/net/udp.h"
#include "src/net/tcp.h"
#include "src/net/net.h"
#include "src/api/net_api.h"
#include "src/endiantool.h"

namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static ConfigVar<uint32_t>::ptr g_sock_buf_max_size =
    Config::look_up("ip.sock_buf_max_size", (uint32_t)1024, "sock队列大小");


static tinytcp::ConfigVar<uint32_t>::ptr g_socket_max_size =
    tinytcp::Config::look_up("socket.max_size", 1024U, "最多能分配多少个socket");

static socket_t* g_socket_tbl;


static int get_index(socket_t* sock) {
    return (int)(sock - g_socket_tbl);
}

static socket_t* get_socket(int idx) {
    if (idx < 0 || idx >= g_socket_max_size->value()) {
        return nullptr;
    }
    return g_socket_tbl + idx;
}

static socket_t* socket_alloc() noexcept {
    for (int i = 0; i < g_socket_max_size->value(); ++i) {
        socket_t* curr = g_socket_tbl + i;
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
    delete sock->sock;
}

net_err_t Sock::close() {
    return net_err_t::NET_ERR_OK;
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

Sock* Sock::find_sock(const ipaddr_t& src, const ipaddr_t& dest, int protocol) {
    for (int i = 0; i < g_socket_max_size->value(); ++i) {
        if (g_socket_tbl[i].state == socket_t::SOCKET_STATE_USED
            && g_socket_tbl[i].sock != nullptr) {
            Sock* sock = g_socket_tbl[i].sock;
            if (sock->m_protocol && sock->m_protocol != protocol) {
                continue;
            }
            if (!sock->m_remote_ip.is_empty() && !(sock->m_remote_ip == src)) {
                continue;
            }
            if (!sock->m_local_ip.is_empty() && !(sock->m_local_ip == dest)) {
                continue;
            }
            return sock;
        }
    }
    return nullptr;
}

net_err_t Sock::setopt(int level, int optname,
                            const char* optval, int optlen) {

    if (level != SOL_SOCKET) {
        TINYTCP_LOG_ERROR(g_logger) << "unknown level";
        return net_err_t::NET_ERR_PARAM;
    }

    switch (optname) {
        case SO_RCVTIMEO:
        case SO_SNDTIMEO: {
            if (optlen != sizeof(timeval)) {
                TINYTCP_LOG_ERROR(g_logger) << "unknown level";
                return net_err_t::NET_ERR_PARAM;
            }
            timeval* time = (timeval*)optval;
            int time_ms = time->tv_sec * 1000 + time->tv_usec / 1000;
            if (optname == SO_RCVTIMEO) {
                m_recv_timeout = time_ms;
                return net_err_t::NET_ERR_OK;
            }
            else if (optname == SO_SNDTIMEO) {
                m_send_timeout = time_ms;
                return net_err_t::NET_ERR_OK;
            }
            else {
                return net_err_t::NET_ERR_PARAM;
            }
            break;
        }
        default: {
            break;
        }
    }

    return net_err_t::NET_ERR_PARAM;
}


bool Sock::push_buf(PktBuffer::ptr& buf) {
    return m_buf_queue.push(buf, 0);
}

PktBuffer::ptr Sock::pop_buf(int timeout) {
    PktBuffer::ptr buf;
    if (timeout < 0) {
        timeout = -1;
    }
    bool ok = m_buf_queue.pop(&buf, timeout);
    if (ok) {
        buf->reset_access();
        return buf;
    }
    return nullptr;
}

net_err_t Sock::connect(sockaddr* addr, socklen_t addr_len) {
    sockaddr_in* remote = (sockaddr_in*)addr;
    m_remote_ip = ipaddr_t(remote->sin_addr.s_addr);
    m_remote_port = net_to_host(remote->sin_port);
    return net_err_t::NET_ERR_OK;
}

net_err_t Sock::bind(sockaddr* addr, socklen_t addr_len) {

    return net_err_t::NET_ERR_OK;
}

net_err_t Sock::sock_bind(const ipaddr_t& ip, uint16_t port) {
    if (!(ip == ipaddr_t(0U))) {
        auto ipproto = ProtocolStackMgr::get_instance()->get_ipprotocol();
        route_entry_t* rt = ipproto->route_find(ip);
        if (!rt || !(rt->netif->get_ipaddr() == ip)) {
            TINYTCP_LOG_ERROR(g_logger) << "addr error";
            return net_err_t::NET_ERR_PARAM;
        }
    }
    m_local_ip = ip;
    m_local_port = port;

    return net_err_t::NET_ERR_OK;
}

net_err_t Sock::send(const void* buf, size_t len, int flags, ssize_t* result_len) {
    sockaddr_in dest;
    dest.sin_family = m_family;
    dest.sin_port = host_to_net(m_remote_port);
    dest.sin_addr.s_addr = m_remote_ip.q_addr;
    return sendto(buf, len, flags, (const struct sockaddr*)&dest, sizeof(dest), result_len);
}

net_err_t Sock::recv(void* buf, size_t len, int flags, ssize_t* result_len) {
    sockaddr src;
    socklen_t addr_len;
    return recvfrom(buf, len, flags, &src, addr_len, result_len);
}


Sock::Sock(int family, int protocol)
    : m_family(family)
    , m_protocol(protocol)
    , m_buf_queue(g_sock_buf_max_size->value()){
    m_local_ip.q_addr = 0;
    m_remote_ip.q_addr = 0;
}

net_err_t socket_init() {
    g_socket_tbl = new socket_t[g_socket_max_size->value()];

    return net_err_t::NET_ERR_OK;
}

net_err_t socket_close_req_in(sock_req_t* req) {
    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }
    net_err_t err = s->sock->close();
    if (err == net_err_t::NET_ERR_NEED_WAIT) {
        auto conn_wait = s->sock->get_conn_wait();
        if (conn_wait) {
            conn_wait->wait_add(s->sock->get_recv_timeout(), req);
        }
    }
    // socket_free(s);
    return err;
}

net_err_t socket_free_req_in(sock_req_t* req) {
    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }
    socket_free(s);
    return net_err_t::NET_ERR_OK;
}

net_err_t socket_setsocket_req_in(sock_req_t* req) {
    socket_t * s = get_socket(req->sockfd);
    if (s== nullptr) {
        TINYTCP_LOG_ERROR(g_logger) << "no socket";
        return net_err_t::NET_ERR_PARAM;
    }
    Sock* sock = s->sock;
    sock_opt_t* opt = &req->opt;

    return sock->setopt(opt->level, opt->optname, opt->optval, opt->optlen);
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
            sock->sock->init();
            break;
        }
        case SOCK_DGRAM: {
            sock->sock = new UDPSock(family, protocol);
            sock->sock->init();
            break;
        }
        case SOCK_STREAM: {
            sock->sock = new TCPSock(family, protocol);
            sock->sock->init();
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

net_err_t socket_send_req_in(sock_req_t* req) {
    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }

    Sock* sock = s->sock;
    net_err_t err = sock->send(req->data.buf, req->data.len, req->data.flags, &req->data.comp_len);
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

net_err_t socket_recv_req_in(sock_req_t* req) {
    socket_t* s = get_socket(req->sockfd);
    if (!s) {
        TINYTCP_LOG_ERROR(g_logger) << "param error";
        return net_err_t::NET_ERR_PARAM;
    }

    Sock* sock = s->sock;
    net_err_t err = sock->recv(req->data.buf, req->data.len, req->data.flags, &req->data.comp_len);
    if (err == net_err_t::NET_ERR_NEED_WAIT) {
        auto recv_wait = sock->get_recv_wait();
        if (recv_wait) {
            recv_wait->wait_add(sock->get_recv_timeout(), req);
        }
    }

    return net_err_t::NET_ERR_OK;
}



#define GET_SOCKET                                        \
    socket_t* s = get_socket(req->sockfd);                \
    if (!s) {                                             \
        TINYTCP_LOG_ERROR(g_logger) << "param error";     \
        return net_err_t::NET_ERR_PARAM;                  \
    }

net_err_t socket_connect_req_in(sock_req_t* req) {
    GET_SOCKET;

    Sock* sock = s->sock;

    net_err_t err = sock->connect(req->conn.addr, req->conn.addr_len);
    if (err == net_err_t::NET_ERR_NEED_WAIT) {
        auto conn_wait = sock->get_conn_wait();
        if (conn_wait) {
            conn_wait->wait_add(sock->get_recv_timeout(), req);
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t socket_bind_req_in(sock_req_t* req) {
    GET_SOCKET;

    Sock* sock = s->sock;

    return sock->bind(req->bind.addr, req->bind.addr_len);
}


} // namespace tinytcp

