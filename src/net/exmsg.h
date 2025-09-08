#pragma once
#include <inttypes.h>
#include <functional>
#include <variant>
#include "netif.h"
#include "src/mutex.h"


namespace tinytcp {

// 网卡相关的具体信息
struct msg_netif_t {
    INetIF* netif;

    msg_netif_t() : netif(nullptr) {}
    msg_netif_t(INetIF* _netif) : netif(_netif) {}
    ~msg_netif_t()  = default;
};

// 函数消息
struct msg_func_t {
    std::function<net_err_t()> func;
    net_err_t err;
    Semaphore sem;

    msg_func_t() : func(nullptr) {}
    msg_func_t(std::function<net_err_t()>& cb) : func(std::move(cb)) {}
    ~msg_func_t() {
        std::function<net_err_t()> empty;
        func.swap(empty);
    }
};

struct exmsg_t {
    using ptr = std::shared_ptr<exmsg_t>;

    enum EXMSGTYPE {
        NET_EXMSG_NETIF_IN,
        NET_EXMSG_EXEC_FUNC,    // 外部调用
        NET_EXMSG_TIMER_FUNC,  // 定时器调用
    };

    EXMSGTYPE type;
    // std::variant<msg_netif_t, msg_func_t> data;
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
    // void set_func(std::function<void()>& func) {
    //     type = NET_EXMSG_func_FUN;
    //     data = msg_func_t(func);
    // }

    union {
        msg_netif_t netif;
        msg_func_t func;
    };

    exmsg_t() : type(NET_EXMSG_NETIF_IN) {
        new (&func) msg_func_t();
    }

    ~exmsg_t() {
        if (type == NET_EXMSG_TIMER_FUNC || type == NET_EXMSG_EXEC_FUNC) {
            func.~msg_func_t();
        } else {
            netif.~msg_netif_t();
        }
    }

    exmsg_t(const exmsg_t&) = delete;
    exmsg_t& operator=(const exmsg_t&) = delete;
};

} // namespace tinytcp


