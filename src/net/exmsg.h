#pragma once
#include <inttypes.h>


namespace tinytcp {

struct exmsg_t {

    enum class EXMSGTYPE : int8_t {
        NET_EXMSG_NETIF_IN,
    };

    EXMSGTYPE type;
    uint32_t id;
};

} // namespace tinytcp


