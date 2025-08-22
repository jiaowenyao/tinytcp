#pragma once
#include <inttypes.h>
#include "netif.h"


namespace tinytcp {

// 网卡相关的具体信息
struct msg_netif_t {
    INetIF* netif;
};

struct exmsg_t {

    enum EXMSGTYPE {
        NET_EXMSG_NETIF_IN,
    };

    EXMSGTYPE type;

    union {
        msg_netif_t netif;
    };
};

} // namespace tinytcp


