#include "sock.h"
#include "src/config.h"
#include "src/log.h"

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
    sockfd = get_index(sock);
    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp

