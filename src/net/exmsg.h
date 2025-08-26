#pragma once
#include <inttypes.h>
#include <functional>
#include <variant>
#include "netif.h"


namespace tinytcp {

// 网卡相关的具体信息
struct msg_netif_t {
    INetIF* netif;

    msg_netif_t() : netif(nullptr) {}
    msg_netif_t(INetIF* _netif) : netif(_netif) {}
    ~msg_netif_t()  = default;
};

// 定时器消息
struct msg_timer_t {
    std::function<void()> func;

    msg_timer_t() : func(nullptr) {}
    msg_timer_t(std::function<void()>& cb) : func(std::move(cb)) {}
    ~msg_timer_t() {
        std::function<void()> empty;
        func.swap(empty);
    }
};

struct exmsg_t {
    enum EXMSGTYPE {
        NET_EXMSG_NETIF_IN,
        NET_EXMSG_TIMER_FUN,
    };

    EXMSGTYPE type;
    // std::variant<msg_netif_t, msg_timer_t> data;
    //
    // exmsg_t() : type(NET_EXMSG_NETIF_IN), data(msg_netif_t{}) {}
    // ~exmsg_t() = default;
    // exmsg_t(const exmsg_t&) = delete;
    // exmsg_t& operator=(const exmsg_t&) = delete;
    //
    // void set_netif(INetIF* netif) {
    //     type = NET_EXMSG_NETIF_IN;
    //     data = msg_netif_t(netif);
    // }
    //
    // void set_timer(std::function<void()>& func) {
    //     type = NET_EXMSG_TIMER_FUN;
    //     data = msg_timer_t(func);
    // }

    union {
        msg_netif_t netif;
        msg_timer_t timer;
    };

    exmsg_t() : type(NET_EXMSG_NETIF_IN) {
        new (&timer) msg_timer_t();
    }

    ~exmsg_t() {
        if (type == NET_EXMSG_TIMER_FUN) {
            timer.~msg_timer_t();
        } else {
            netif.~msg_netif_t();
        }
    }

    exmsg_t(const exmsg_t&) = delete;
    exmsg_t& operator=(const exmsg_t&) = delete;
};

} // namespace tinytcp


