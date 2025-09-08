#pragma once

#include "net_err.h"
#include <memory>

namespace tinytcp {

struct socket_t {
    using ptr = std::shared_ptr<socket_t>;

    enum {
        SOCKET_STATE_FREE,
        SOCKET_STATE_USED,
    } state;

};

net_err_t socket_init();
net_err_t socket_create_req_in(int family, int type, int protocol, int& sockfd);

} // namespace tinytcp

